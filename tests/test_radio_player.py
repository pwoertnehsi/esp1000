import ctypes
import ctypes.util
from pathlib import Path
import re
import struct
import unittest

from tools.prepare_radio_player import rewrite, LOCAL_URL

ROOT = Path(__file__).resolve().parents[1]


def template(properties):
    nodes = bytearray()
    strings = bytearray()
    for key, value in properties:
        key_offset = len(strings)
        strings += key.encode() + b'\0'
        value_offset = len(strings)
        encoded = value.encode()
        strings += encoded + b'\0'
        nodes += struct.pack('<4I', key_offset, 3, value_offset, len(encoded))
    start = 64 + len(nodes)
    header = bytearray(64)
    header[:8] = b'PRSF\0\1\0\0'
    struct.pack_into('<II', header, 8, 64, len(nodes))
    struct.pack_into('<II', header, 24, start, len(strings))
    return bytes(header + nodes + strings + b'PRIVATE-ICON-UNCHANGED')


class RadioPlayerTest(unittest.TestCase):
    def test_private_template_preserved(self):
        data = template([('title', 'An existing radio player'), ('radioplayer_url', 'http://example.invalid/' + 'x' * 80)])
        result = rewrite(data, {'title': 'ESP1000 Direct Radio', 'radioplayer_url': LOCAL_URL})
        self.assertEqual(len(result), len(data))
        self.assertEqual(result[:64], data[:64])
        self.assertTrue(result.endswith(b'PRIVATE-ICON-UNCHANGED'))
        self.assertIn(LOCAL_URL.encode() + b'\0', result)
        self.assertIn(b'An existing radio player', data)

    def test_reject_bad_tables_and_ambiguous_strings(self):
        data = template([('title', 'Original')])
        with self.assertRaises(ValueError):rewrite(data, {'title': 'Too long to fit'})
        with self.assertRaises(ValueError):rewrite(data, {'title': 'a\0b'})
        with self.assertRaises(ValueError):rewrite(data, {'absent': 'x'})
        duplicate = template([('title', 'Original'), ('title', 'Duplicate')])
        with self.assertRaises(ValueError):rewrite(duplicate, {'title': 'x'})
        bad = bytearray(data);struct.pack_into('<I', bad, 28, 0xffffffff)
        with self.assertRaises(ValueError):rewrite(bytes(bad), {'title': 'x'})
        bad = bytearray(data);struct.pack_into('<I', bad, 64 + 8, 0xffffffff)
        with self.assertRaises(ValueError):rewrite(bytes(bad), {'title': 'x'})

    def test_native_api_controls(self):
        library = ctypes.util.find_library('duktape')
        if not library:self.skipTest('Duktape JavaScript engine unavailable')
        js = ctypes.CDLL(library)
        js.duk_create_heap.argtypes = [ctypes.c_void_p] * 5
        js.duk_create_heap.restype = ctypes.c_void_p
        js.duk_get_global_string.argtypes = [ctypes.c_void_p, ctypes.c_char_p]
        js.duk_push_lstring.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
        js.duk_push_lstring.restype = ctypes.c_char_p
        js.duk_pcall.argtypes = [ctypes.c_void_p, ctypes.c_int]
        js.duk_safe_to_lstring.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.POINTER(ctypes.c_size_t)]
        js.duk_safe_to_lstring.restype = ctypes.c_char_p
        js.duk_destroy_heap.argtypes = [ctypes.c_void_p]
        script = re.search(r'<script[^>]*>(.*?)</script>', (ROOT/'radio_player/index.html').read_text(), re.S).group(1)
        harness = '''
var calls=[],nodes={};
function assert(x){if(!x)throw Error('radio control assertion');}
var api={sysRadioSetMasterVolume:function(v){calls.push(['master',v]);},sysRadioSetSubVolume:function(v){calls.push(['sub',v]);},
sysRadioPlayStream:function(url,agent){calls.push(['play',url,agent]);return 0;},sysRadioStop:function(){calls.push(['stop']);},
sysRadioGetPlayerStatus:function(){return 7;},sysRadioGetStreamTitle:function(){return '<test title>';}};
function node(id){if(!nodes[id])nodes[id]={firstChild:null,value:'',removeChild:function(){this.firstChild=null;},appendChild:function(t){this.firstChild=t;}};return nodes[id];}
var document={psp:api,getElementById:node,createTextNode:function(t){return {nodeValue:t};}};
var window={};function setTimeout(){return 1;}function setInterval(){return 2;}function clearInterval(){}
'''
        checks = '''
init();assert(nodes.status.firstChild.nodeValue.indexOf('Ready')==0);
nodes.url={value:'http://example.invalid/test.mp3'};play();
assert(calls[0][0]=='master' && calls[0][1]==255 && calls[1][1]==255);
assert(calls[2][0]=='play' && calls[2][1]==nodes.url.value && timer==2);
update();assert(nodes.details.firstChild.nodeValue=='Native status: 7 | <test title>');
stop();assert(timer===null && calls[3][0]=='stop');
nodes.url.value='javascript:alert(1)';play();assert(calls.length==4);
assert(nodes.status.firstChild.nodeValue.indexOf('Enter an HTTP')>=0);
api.sysRadioPlayStream=function(){throw Error('plugin refused');};nodes.url.value='https://example.invalid/live';play();
assert(nodes.status.firstChild.nodeValue.indexOf('plugin refused')>=0);
'''
        context = js.duk_create_heap(None, None, None, None, None)
        self.assertTrue(context)
        try:
            code = (harness + script + checks).encode()
            js.duk_get_global_string(context, b'eval')
            js.duk_push_lstring(context, code, len(code))
            result = js.duk_pcall(context, 1)
            error = js.duk_safe_to_lstring(context, -1, None).decode(errors='replace')
            self.assertEqual(result, 0, error)
        finally:js.duk_destroy_heap(context)
