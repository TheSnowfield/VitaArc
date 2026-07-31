#ifndef VITAARC_JNI_NAMES_H
#define VITAARC_JNI_NAMES_H

/* Core classes. */
#define JCLS_OBJECT       "java/lang/Object"
#define JCLS_CLASS        "java/lang/Class"
#define JCLS_LOADER       "java/lang/ClassLoader"
#define JCLS_STRING       "java/lang/String"
#define JCLS_FILE         "java/io/File"
#define JCLS_BUFFER       "java/nio/ByteBuffer"
#define JCLS_CLONEABLE    "java/lang/Cloneable"
#define JCLS_SERIALIZABLE "java/io/Serializable"

/* Exception hierarchy. */
#define JEX_THROWABLE     "java/lang/Throwable"
#define JEX_BASE          "java/lang/Exception"
#define JEX_RUNTIME       "java/lang/RuntimeException"
#define JEX_IO            "java/io/IOException"
#define JEX_NULL          "java/lang/NullPointerException"
#define JEX_ARG           "java/lang/IllegalArgumentException"
#define JEX_STATE         "java/lang/IllegalStateException"
#define JEX_MEMORY        "java/lang/OutOfMemoryError"
#define JEX_MONITOR       "java/lang/IllegalMonitorStateException"
#define JEX_STORE         "java/lang/ArrayStoreException"
#define JEX_ARRAY_SIZE    "java/lang/NegativeArraySizeException"
#define JEX_UNSUPPORTED   "java/lang/UnsupportedOperationException"
#define JEX_BOUNDS        "java/lang/IndexOutOfBoundsException"
#define JEX_ARRAY_BOUNDS  "java/lang/ArrayIndexOutOfBoundsException"
#define JEX_STRING_BOUNDS "java/lang/StringIndexOutOfBoundsException"
#define JEX_LINK          "java/lang/UnsatisfiedLinkError"

#endif
