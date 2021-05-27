#ifndef _BRIDGE_ANDROID_LOGIMPL_H_
#define _BRIDGE_ANDROID_LOGIMPL_H_

int __android_log_print(int prio, const char *tag, const char *format, ...);

int __android_log_write(int prio, const char *tag, const char *format, ...);

#endif /* _BRIDGE_ANDROID_LOGIMPL_H_ */
