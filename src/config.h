#ifndef __CONFIG_H
#define __CONFIG_H

#define PATH_TO_APP "app0:"
#define PATH_TO_PACKAGE PATH_TO_APP "package/"
#define PATH_TO_LIBRARY PATH_TO_PACKAGE "lib/armeabi-v7a/"
#define PATH_TO_ASSETS PATH_TO_PACKAGE "assets"
#define PATH_TO_OBB PATH_TO_PACKAGE "obb/"

#define LIBRARY_LIBCOCOS2DCPP PATH_TO_LIBRARY "libcocos2dcpp.so"

#define PATH_TO_USER "ux0:vitaarc/"
#define PATH_TO_LOGFILE PATH_TO_USER "boot.log"
#define PATH_TO_DATA PATH_TO_USER "persistent/data/"

#endif /* __CONFIG_H */
