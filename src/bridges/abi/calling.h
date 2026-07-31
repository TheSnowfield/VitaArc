#ifndef __BRIDGES_ABI_CALLING_H
#define __BRIDGES_ABI_CALLING_H

#if defined(__arm__)
#define VITAARC_SOFTFP __attribute__((pcs("aapcs")))
#else
#define VITAARC_SOFTFP
#endif

#endif /* __BRIDGES_ABI_CALLING_H */
