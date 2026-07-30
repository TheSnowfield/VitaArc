#ifndef __BRIDGES_ANDROID_LOG_H
#define __BRIDGES_ANDROID_LOG_H

int __android_log_print(int prio, const char *tag, const char *format, ...);

int __android_log_write(int prio, const char *tag, const char *format, ...);

#endif /* __BRIDGES_ANDROID_LOG_H */
