#ifndef PK_H
#define PK_H

#define PK()          PKF("", 0)
#define PKS(str)      PKF("%s", str)
#define PKF(fmt, ...) printk(KERN_EMERG "ATTIE: %s:%d %s(): " fmt, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__)

#endif /* PK_H */
