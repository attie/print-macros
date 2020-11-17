#ifndef PK_H
#define PK_H

#define PK()               PKR("")
#define PKS(str)           PKR(": %s", str)
#define PKF(fmt, args...)  PKR(": " fmt, ##args)
#define PKV(fmt, var)      PKR(": " #var ": " fmt, var)
#define PKVB(fmt, var)     PKR(": " #var ": [" fmt "]", var)
#define PKR(fmt, args...)  printk(KERN_INFO "ATTIE: %s:%d %s()" fmt, __FILE__, __LINE__, __FUNCTION__, ##args)

#endif /* PK_H */
