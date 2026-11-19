#ifndef WIFI_MENU_RUNTIME_H
#define WIFI_MENU_RUNTIME_H
typedef struct {
    unsigned serial,version[2],attribute,text,size,segments;
    unsigned segment_address[4],segment_size[4];
    unsigned anchor_count,anchor_words[16],row_count,row_masks[16];
    unsigned bridge_callbacks[2];
    int result,user_address;
    unsigned menu_instance,menu_parent,category_count,item_counts[8],root_resources[8],network_resources[8];
} wifi_menu_snapshot_t;
void wifi_menu_snapshot(wifi_menu_snapshot_t *out);
int wifi_menu_init(void);
int wifi_menu_stop(void);
int wifi_menu_can_stop(void);
unsigned wifi_menu_poll(void);
int wifi_menu_launch(unsigned command);
int wifi_menu_status(void);
int wifi_menu_exit_status(void);
int wifi_menu_verify(void);
int wifi_menu_activity(unsigned out[3]);
#endif
