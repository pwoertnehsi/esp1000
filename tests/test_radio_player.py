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
        script = '\n'.join(re.findall(r'<script[^>]*>(.*?)</script>', (ROOT/'radio_player/index.html').read_text(), re.S))
        harness = '''
var calls=[];var fields={statusText:{value:''},detailsText:{value:''},streamUrl:{value:'http://example.invalid/test.mp3'},source:{value:'https://example.invalid/alternate'}};
function assert(x){if(!x)throw Error('radio control assertion');}
var api={sysRadioSetMasterVolume:function(v){calls.push(['master',v]);},sysRadioSetSubVolume:function(v){calls.push(['sub',v]);},
sysRadioPlayStream:function(url,agent){calls.push(['play',url,agent]);return 0;},sysRadioStop:function(){calls.push(['stop']);},
sysRadioGetPlayerStatus:function(){return 1;},sysRadioGetStreamBufferLevel:function(){return 45;},sysRadioGetStreamTitle:function(){return '<test title>';}};
var psp=api;
// Deliberately no getElementById, createTextNode, appendChild or document.psp.
var document={radioControls:{elements:fields}};
var window={};function setTimeout(){return 1;}function setInterval(){return 2;}function clearInterval(){}
'''
        checks = '''
assert(fields.statusText.value.indexOf('JavaScript is running')==0);
assert(espRadioInit() && fields.statusText.value.indexOf('Ready')==0);
espRadioPlay();
assert(calls[0][0]=='master' && calls[0][1]==255 && calls[1][1]==255);
assert(calls[2][0]=='play' && calls[2][1]==fields.streamUrl.value && espRadioTimer==2);
assert(calls[3][0]=='sub' && calls[3][1]==255);
assert(fields.statusText.value.indexOf('Request queued (0)')==0);
assert(fields.detailsText.value.indexOf('Native: 1 (playing) | Buffer: 45')==0);
espRadioUpdate();assert(fields.detailsText.value=='Native: 1 (playing) | Buffer: 45 | <test title> | Sample: 2');
espRadioStop();assert(espRadioTimer===null && calls[4][0]=='stop');
fields.streamUrl.value='javascript:alert(1)';espRadioPlay();assert(calls.length==5);
assert(fields.statusText.value.indexOf('Enter an HTTP')>=0);
api.sysRadioPlayStream=function(){throw Error('plugin refused');};fields.streamUrl.value='https://example.invalid/live';espRadioPlay();
assert(fields.statusText.value.indexOf('plugin refused')>=0);
espRadioPreset();assert(fields.streamUrl.value==fields.source.value);
api.sysRadioGetPlayerStatus=function(){return 4;};api.sysRadioGetStreamBufferLevel=function(){return 0;};
espRadioUpdate();assert(fields.detailsText.value.indexOf('Native: 4 (connecting) | Buffer: 0')==0);
api.sysRadioGetPlayerStatus=function(){return -1;};espRadioUpdate();assert(fields.detailsText.value.indexOf('Native: -1 (error)')==0);
api.sysRadioGetPlayerStatus=function(){return 1;};api.sysRadioGetStreamBufferLevel=function(){return 45;};
psp=null;assert(!espRadioInit() && espRadio===null && fields.statusText.value.indexOf('Initialize: ')==0);
espRadioPlay();assert(fields.statusText.value.indexOf('Initialize: ')==0);
psp=api;assert(espRadioInit());
api.sysRadioGetStreamTitle=function(){throw Error('title unavailable');};espRadioUpdate();assert(fields.detailsText.value.indexOf('Native: 1 (playing) | Buffer: 45 | Sample: ')==0);
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
