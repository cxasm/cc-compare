TEMPLATE = app
LANGUAGE	= C++

TARGET = CCompare

CONFIG	+= qt warn_on release static

QT += core gui widgets concurrent sql network

HEADERS	+= *.h 
		
SOURCES	+= *.cpp *.cc  \
    rcglobal.cpp
		
FORMS += *.ui 

RESOURCES += RealCompare.qrc

ICON = main.icns

INCLUDEPATH	+= qscint/src
INCLUDEPATH	+= qscint/src/Qsci
INCLUDEPATH	+= qscint/scintilla/include

DEFINES +=  QSCINTILLA_DLL

TRANSLATIONS += realcompare_zh.ts
	
win32 {
   if(contains(QMAKE_HOST.arch, x86_64)){
    CONFIG(Debug, Debug|Release){
        DESTDIR = x64/Debug
		LIBS	+= -Lx64/Debug
		LIBS += -lqmyedit_qt5d
    }else{
        DESTDIR = x64/Release
		LIBS	+= -Lx64/Release
		LIBS += -lqmyedit_qt5
    }
   }
}


win32{
	if(contains(QMAKE_HOST.arch, x86_64)){
		if(CONFIG(Debug, Debug|Release)){
			LIBS += -Llib64/Debug -llibprotobufd
		}else{
			LIBS += -Llib64/Release -llibprotobuf
		}
   }else{
		if(CONFIG(Debug, Debug|Release)){
			LIBS += -Llib32/Debug -llibprotobufd
		}else{
			LIBS += -Llib32/Release -llibprotobuf
		}
	}
}

unix{

if(CONFIG(Debug, Debug|Release)){
          LIBS += -L/home/yzw/build/cccompare/lib -lprotobuf
          LIBS += -L/home/yzw/build/cccompare/x64/Debug -lqmyedit_qt5_debug
}else{
          LIBS += -L/home/yzw/build/cccompare/lib -lprotobuf
          LIBS += -L/home/yzw/build/cccompare/x64/Release -lqmyedit_qt5
          DESTDIR = x64/Release

        QMAKE_CXXFLAGS += -fopenmp -O2
        LIBS += -lgomp -lpthread
}
}

win32
{
INCLUDEPATH += e://protobuf-3.11.4/src
}
unix
{
INCLUDEPATH +=/home/yzw/build/protobuf-3.11.4/output/include
}

RC_FILE += RealCompare.rc

DISTFILES += \
    RealCompare.rc
