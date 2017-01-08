QT       += core gui

TARGET = AdaViewer
TEMPLATE = app

INCLUDEPATH += ../NAF

	DESTDIR = ./tmp
	OBJECTS_DIR = ./tmp
	CONFIG(debug, debug|release) {
		DESTDIR = ./tmp-dbg
		OBJECTS_DIR = ./tmp-dbg
		DEFINES += _DEBUG
	}
	RCC_DIR = ./tmp
	UI_DIR = ./tmp
	MOC_DIR = ./moc

SOURCES +=\
        AdaViewer.cpp \
    AdaLexer.cpp \
    AdaHighlighter.cpp \
    AdaEditor.cpp

HEADERS  += AdaViewer.h \
    AdaLexer.h \
    AdaHighlighter.h \
    AdaEditor.h

!include(../NAF/Gui2/Gui2.pri) {
	 message( "Missing NAF Gui2" )
 }
