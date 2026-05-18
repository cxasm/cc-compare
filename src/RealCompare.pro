TEMPLATE = app
LANGUAGE	= C++

TARGET = CCompare

CONFIG	+= qt warn_on release static

CONFIG -= import_plugins

QT += core gui widgets concurrent sql network

HEADERS	+= *.h 
		
SOURCES	+= *.cpp *.cc 
		
FORMS += *.ui 

RESOURCES += RealCompare.qrc

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
unix{
   if(contains(QMAKE_HOST.arch, x86_64)){
    CONFIG(Debug, Debug|Release){
        DESTDIR = x64/Debug
		LIBS	+= -Lx64/Debug
		LIBS += -lqmyedit_qt5
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
			LIBS += -Llib64/Debug  -luchardet
		}else{
			LIBS += -Llib64/Release  -luchardet
		}
   }else{
		if(CONFIG(Debug, Debug|Release)){
			LIBS += -Llib32/Debug -luchardet
		}else{
			LIBS += -Llib32/Release  -luchardet
			}
  }
}


unix{
	QMAKE_CXXFLAGS += -fopenmp
}

win32{
	QMAKE_CXXFLAGS += /openmp
}

RC_FILE += RealCompare.rc
