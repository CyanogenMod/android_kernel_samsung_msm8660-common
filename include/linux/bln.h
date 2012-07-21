/* include/linux/bln.h */

#ifndef _LINUX_BLN_H
#define _LINUX_BLN_H

struct bln_implementation {
    bool (*enable)(void);
    void (*disable)(void);
    void (*on)(void);
    void (*off)(void);
};

void register_bln_implementation(struct bln_implementation *imp);
void cancel_bln_activity(void);
#endif
