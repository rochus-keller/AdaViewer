/*
* Copyright 2012-2017 Rochus Keller <mailto:me@rochus-keller.info>
*
* This file is part of the AdaViewer application.
*
* The following is the license that applies to this copy of the
* application. For a license to use the application under conditions
* other than those described here, please email to me@rochus-keller.info.
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

#include "AdaEditor.h"
#include "AdaHighlighter.h"
#include <Gui2/AutoMenu.h>
#include <QPainter>
#include <QtDebug>
#include <QFile>
#include <QScrollBar>
#include <QBuffer>
#include <QApplication>
#include <QClipboard>
#include <QInputDialog>
#include <QPrinter>
#include <QFileDialog>
#include <QFileInfo>
#include <QPrintDialog>
#include <QSettings>
#include <QFontDialog>
#include <QShortcut>

// adaptiert aus Lua::CodeEditor

namespace Ada
{
static const int s_charPerTab = 3; // TODO: Einstellbar

class _HandleArea : public QWidget
{
public:
	_HandleArea(Editor *editor) : QWidget(editor), d_start(-1)
    {
        d_codeEditor = editor;
    }

    QSize sizeHint() const
    {
        return QSize(d_codeEditor->handleAreaWidth(), 0);
    }

protected:
    void paintEvent(QPaintEvent *event)
    {
        // Weil die meisten Funktionen protected sind, delegieren wir die Arbeit an den Editor.
        d_codeEditor->paintHandleArea(event);
    }
    void mousePressEvent(QMouseEvent * event)
    {
        if( event->buttons() != Qt::LeftButton )
            return;
        if( event->modifiers() == Qt::ShiftModifier )
        {
            QTextCursor cur = d_codeEditor->textCursor();
            const int selStart = d_codeEditor->document()->findBlock( cur.selectionStart() ).blockNumber();
            const int selEnd = d_codeEditor->document()->findBlock( cur.selectionEnd() ).blockNumber();
            Q_ASSERT( selStart <= selEnd ); // bei position und anchor nicht erfllt
            const int clicked = d_codeEditor->lineAt( event->pos() );
            if( clicked <= selEnd )
            {
                if( cur.selectionStart() == cur.position() )
                    d_codeEditor->selectLines( selEnd, clicked );
                else
                    d_codeEditor->selectLines( selStart, clicked );
            }else
                d_codeEditor->selectLines( selStart, clicked );
        }else
        {
            // Beginne Drag
            d_start = d_codeEditor->lineAt( event->pos() );
            d_codeEditor->selectLines( d_start, d_start );
        }
    }
    void mouseMoveEvent(QMouseEvent * event)
    {
        if( d_start != -1 )
        {
            const int cur = d_codeEditor->lineAt( event->pos() );
            d_codeEditor->selectLines( d_start, cur );
        }
    }
    void mouseReleaseEvent(QMouseEvent * event )
    {
        if( d_start != -1 )
        {
            const int cur = d_codeEditor->lineAt( event->pos() );
            d_codeEditor->selectLines( d_start, cur );
            d_start = -1;
        }
    }
    void mouseDoubleClickEvent(QMouseEvent * event)
    {
        if( event->buttons() != Qt::LeftButton )
            return;
        const int line = d_codeEditor->lineAt( event->pos() );
        d_codeEditor->numberAreaDoubleClicked( line );
    }

private:
	Editor* d_codeEditor;
    int d_start;
};
}
using namespace Ada;

Editor::Editor(QWidget *parent) :
	QPlainTextEdit(parent), d_showNumbers(true),
    d_undoAvail(false),d_redoAvail(false),d_copyAvail(false),d_curPos(-1)
{
	setFont( defaultFont() );
    setLineWrapMode( QPlainTextEdit::NoWrap );
    setTabStopWidth( 30 );
    setTabChangesFocus(false);

    d_numberArea = new _HandleArea(this);

    connect(this, SIGNAL(blockCountChanged(int)), this, SLOT(updateLineNumberAreaWidth()));
    connect(this, SIGNAL(updateRequest(QRect,int)), this, SLOT(updateLineNumberArea(QRect,int)));
    connect(this, SIGNAL(cursorPositionChanged()), this, SLOT(highlightCurrentLine()));
    connect(this, SIGNAL(undoAvailable(bool)), this, SLOT(onUndoAvail(bool)) );
    connect(this, SIGNAL(redoAvailable(bool)), this, SLOT(onRedoAvail(bool)) );
    connect( this, SIGNAL(copyAvailable(bool)), this, SLOT(onCopyAvail(bool)) );
	connect( this, SIGNAL( cursorPositionChanged() ), this, SLOT(  onUpdateCursor() ) );

    updateLineNumberAreaWidth();
    highlightCurrentLine();

	new Highlighter( document() );
	updateTabWidth();

	QSettings set;
	const bool showLineNumbers = set.value( "AdaEditor/ShowLineNumbers" ).toBool();
	setShowNumbers( showLineNumbers );
	setFont( set.value( "AdaEditor/Font", QVariant::fromValue( font() ) ).value<QFont>() );
}

QFont Editor::defaultFont()
{
	QFont f;
#ifdef Q_WS_X11
	f.setFamily("Courier");
#else
	f.setStyleHint( QFont::TypeWriter );
#endif
	f.setPointSize(9);
	return f;
}

int Editor::handleAreaWidth()
{
    if( !d_showNumbers )
        return 10; // RISK

    int digits = 1;
    int max = qMax(1, blockCount());
    while (max >= 10) {
        max /= 10;
        ++digits;
    }

    int space = 5 + fontMetrics().width(QLatin1Char('9')) * digits;

    return space;
}

int Editor::lineAt(const QPoint & p) const
{
    const int y =  p.y() - contentOffset().y();
    return cursorForPosition( QPoint( contentsRect().left(), y ) ).blockNumber();
}

void Editor::getCursorPosition(int *line, int *index)
{
    QTextCursor cur = textCursor();
    QTextBlock block = cur.block();
    if( line )
        *line = block.blockNumber();
    if( index )
        *index = cur.position() - block.position() - 1;
}

void Editor::setCursorPosition(int line, int index)
{
    if( line >= 0 && line < document()->blockCount() )
    {
        QTextBlock block = document()->findBlockByNumber(line);
        QTextCursor cur = textCursor();
        cur.setPosition( block.position() + index );
        setTextCursor( cur );
        ensureCursorVisible();
    }
}

int Editor::getTokenTypeAtCursor() const
{
    const QTextCursor cur = textCursor();
    const QTextBlock block = cur.block();
    const int pos = cur.selectionStart() - block.position();
    QList<QTextLayout::FormatRange> fmts = block.layout()->additionalFormats();
    foreach( const QTextLayout::FormatRange& f, fmts )
    {
        if( pos >= f.start && pos < f.start + f.length )
			return f.format.intProperty(Highlighter::TokenProp);
    }
    return 0;
}

QString Editor::textLine(int i) const
{
    if( i < document()->blockCount() )
        return document()->findBlockByNumber(i).text();
    else
		return QString();
}

void Editor::setName(const QString &str)
{
	d_name = str;
	// nicht praktisch da mit setText gel�scht: setDocumentTitle( str )
	emit modificationChanged( document()->isModified() ); // Damit Titel auch in GUI nachgefhrt werden kann
}

int Editor::lineCount() const
{
    return document()->blockCount();
}

void Editor::ensureLineVisible(int line)
{
    if( line >= 0 && line < document()->blockCount() )
    {
        QTextBlock b = document()->findBlockByNumber(line);
        const int p = b.position();
        QTextCursor cur = textCursor();
        cur.setPosition( p );
        setTextCursor( cur );
        ensureCursorVisible();

        // verticalScrollBar()->value() ist Zeilennummer, nicht Pixel!
//        const int visibleLines = document()->blockCount() - verticalScrollBar()->maximum();
//        if( visibleLines > 0 )
//        {
//            if( line < verticalScrollBar()->value() || line > ( verticalScrollBar()->value() + visibleLines ) )
//                verticalScrollBar()->setValue( line );
//        }
    }
}

void Editor::setPositionMarker(int line)
{
    d_curPos = line;
    d_numberArea->update();
    ensureLineVisible( line );
}

void Editor::setSelection(int lineFrom, int indexFrom, int lineTo, int indexTo)
{
    if( lineFrom < document()->blockCount() && lineTo < document()->blockCount() )
    {
        QTextCursor cur = textCursor();
        cur.setPosition( document()->findBlockByNumber(lineFrom).position() + indexFrom );
        cur.setPosition( document()->findBlockByNumber(lineTo).position() + indexTo, QTextCursor::KeepAnchor );
        setTextCursor( cur );
        ensureCursorVisible();
    }
}

void Editor::selectLines(int lineFrom, int lineTo)
{
    if( lineFrom < document()->blockCount() && lineTo < document()->blockCount() )
    {
        QTextCursor cur = textCursor();
        QTextBlock from = document()->findBlockByNumber(lineFrom);
        QTextBlock to = document()->findBlockByNumber(lineTo);
        if( lineFrom < lineTo )
        {
            cur.setPosition( from.position() );
            cur.setPosition( to.position() + to.length() - 1, QTextCursor::KeepAnchor );
        }else
        {
            // Auch wenn sie gleich sind, wird von rechts nach links selektiert, damit kein HoriScroll bei berlnge
            cur.setPosition( from.position() + from.length() - 1 );
            cur.setPosition( to.position(), QTextCursor::KeepAnchor );
        }
        setTextCursor( cur );
    }
}

bool Editor::hasSelection() const
{
    return textCursor().hasSelection();
}

QString Editor::selectedText() const
{
    return textCursor().selectedText();
}

void Editor::updateLineNumberAreaWidth()
{
    setViewportMargins( handleAreaWidth(), 0, 0, 0);
}

void Editor::updateLineNumberArea(const QRect &rect, int dy)
{
    if (dy)
        d_numberArea->scroll(0, dy);
    else
        d_numberArea->update(0, rect.y(), d_numberArea->width(), rect.height());

    if (rect.contains(viewport()->rect()))
		updateLineNumberAreaWidth();
}

void Editor::onUpdateCursor()
{
	int line, col;
	getCursorPosition( &line, &col );
	qDebug() << QString("Line: %1  Column: %2  %3").
							arg(line + 1).arg(col + 1).
							arg( Highlighter::formatTokenType( getTokenTypeAtCursor() ) );
}

void Editor::handleEditUndo()
{
	ENABLED_IF( isUndoAvailable() );
	undo();
}

void Editor::handleEditRedo()
{
	ENABLED_IF( isRedoAvailable() );
	redo();
}

void Editor::handleEditCut()
{
	ENABLED_IF( !isReadOnly() && isCopyAvailable() );
	cut();
}

void Editor::handleEditCopy()
{
	ENABLED_IF( isCopyAvailable() );
	copy();
}

void Editor::handleEditPaste()
{
	QClipboard* cb = QApplication::clipboard();
	ENABLED_IF( !isReadOnly() && !cb->text().isNull() );
	paste();
}

void Editor::handleEditSelectAll()
{
	ENABLED_IF( true );
	selectAll();
}

void Editor::handleFind()
{
	ENABLED_IF( true );
	bool ok	= FALSE;
	QString res = QInputDialog::getText( this, tr("Find Text"),
		tr("Enter a string to look for:"), QLineEdit::Normal, "", &ok );
	if( !ok )
		return;
	d_find = res.toLatin1();
	find( true );
}

void Editor::handleFindAgain()
{
	ENABLED_IF( !d_find.isEmpty() );
	find( false );
}

void Editor::handleReplace()
{
	// TODO
}

void Editor::handleGoto()
{
	ENABLED_IF( true );
	int line, col;
	getCursorPosition( &line, &col );
	bool ok	= FALSE;
	line = QInputDialog::getInteger( this, tr("Goto Line"),
		tr("Please	enter a valid line number:"),
		line + 1, 1, 999999, 1,	&ok );
	if( !ok )
		return;
	setCursorPosition( line - 1, col );
	//ensureLineVisible( line - 1 );
}

void Editor::handleIndent()
{
	ENABLED_IF( !isReadOnly() );

	indent();
}

void Editor::handleUnindent()
{
	ENABLED_IF( !isReadOnly() );

	unindent();
}

void Editor::handleSetIndent()
{
	ENABLED_IF( !isReadOnly() );

	bool ok;
	const int level = QInputDialog::getInteger( this, tr("Set Indentation Level"),
		tr("Enter the indentation level (0..20):"), 0, 0, 20, 1, &ok );
	if( ok )
		setIndentation( level );
}

void Editor::find(bool fromTop)
{
	int line, col;
	getCursorPosition( &line, &col );
	if( fromTop )
	{
		line = 0;
		col = 0;
	}else
		col++;
	const int count = lineCount();
	int j = -1;
	for( int i = qMax(line,0); i < count; i++ )
	{
		j = textLine( i ).indexOf( d_find, col );
		if( j != -1 )
		{
			line = i;
			col = j;
			break;
		}else if( i < count )
			col = 0;
	}
	if( j != -1 )
	{
		setCursorPosition( line, col + d_find.size() );
		ensureLineVisible( line );
		setSelection( line, col, line, col + d_find.size() );
	}

}

void Editor::handlePrint()
{
	ENABLED_IF( true );

	QPrinter p;
	p.setPageMargins( 15, 10, 10, 10, QPrinter::Millimeter );

	QPrintDialog dialog( &p, this );
	if( dialog.exec() )
	{
		print( &p );
	}
}

void Editor::handleExportPdf()
{
	ENABLED_IF( true );

	QString fileName = QFileDialog::getSaveFileName(this, tr("Export PDF"), QString(), tr("*.pdf") );
	if (fileName.isEmpty())
		return;

	QFileInfo info( fileName );

	if( info.suffix().toUpper() != "PDF" )
		fileName += ".pdf";
	info.setFile( fileName );

	QPrinter p;
	p.setPageMargins( 15, 10, 10, 10, QPrinter::Millimeter );
	p.setOutputFormat(QPrinter::PdfFormat);
	p.setOutputFileName(fileName);

	print( &p );
}

void Editor::handleShowLinenumbers()
{
	CHECKED_IF( true, showNumbers() );

	const bool showLineNumbers = !showNumbers();
	QSettings set;
	set.setValue( "AdaEditor/ShowLineNumbers", showLineNumbers );
	setShowNumbers( showLineNumbers );
}

void Editor::handleSetFont()
{
	ENABLED_IF( true );

	bool ok;
	QFont res = QFontDialog::getFont( &ok, font(), this );
	if( !ok )
		return;
	QSettings set;
	set.setValue( "AdaEditor/Font", QVariant::fromValue(res) );
	set.sync();
	setFont( res );
}

void Editor::handleOpen()
{
	ENABLED_IF( true );

	const QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"), QString(),
														  tr("Ada Source (*.adb *.ads)") );
	if (fileName.isEmpty())
		return;

	loadFromFile(fileName);
}

void Editor::resizeEvent(QResizeEvent *e)
{
    QPlainTextEdit::resizeEvent(e);

    QRect cr = contentsRect();
    d_numberArea->setGeometry(QRect(cr.left(), cr.top(), handleAreaWidth(), cr.height()));
}

void Editor::paintEvent(QPaintEvent *e)
{
    QPlainTextEdit::paintEvent( e );
    paintIndents( e );
}

static inline int _firstNwsPos( const QTextBlock& b )
{
    const QString str = b.text();
    for( int i = 0; i < str.size(); i++ )
    {
        if( !str[i].isSpace() )
            return b.position() + i;
    }
    return b.position() + b.length(); // Es ist nur WS vorhanden
}

bool Editor::viewportEvent( QEvent * event )
{
    if( event->type() == QEvent::FontChange )
    {
        updateTabWidth();
        updateLineNumberAreaWidth();
    }
    return QPlainTextEdit::viewportEvent(event);
}

void Editor::keyPressEvent(QKeyEvent *e)
{
    // SHIFT+TAB kommt hier nie an aus nicht nachvollziehbaren Grnden. Auch in event und viewPortEvent nicht.
    // NOTE: Qt macht daraus automatisch BackTab und versendet das!
    if( e->key() == Qt::Key_Tab )
    {
        if( e->modifiers() & Qt::ControlModifier )
        {
            // Wir brauchen CTRL+TAB fr Umschalten der Scripts
            e->ignore();
            return;
        }
        if( !isReadOnly() && ( hasSelection() || textCursor().position() <= _firstNwsPos( textCursor().block() ) ) )
        {
            if( e->modifiers() == Qt::NoModifier )
            {
                indent();
                e->accept();
                return;
            }
        }
	}else if( e->key() == Qt::Key_Backtab )
    {
        if( e->modifiers() & Qt::ControlModifier )
        {
            // Wir brauchen CTRL+TAB fr Umschalten der Scripts
            e->ignore();
            return;
        }
        if( !isReadOnly() && ( hasSelection() || textCursor().position() <= _firstNwsPos( textCursor().block() ) ) )
        {
            unindent();
            e->accept();
            return;
        }
    }else if( !isReadOnly() && ( e->key() == Qt::Key_Enter || e->key() == Qt::Key_Return ) )
    {
        e->accept();
        if( e->modifiers() != Qt::NoModifier )
            return; // Verschlucke Return mit Ctrl etc.
        textCursor().beginEditBlock();
        QPlainTextEdit::keyPressEvent( e ); // Lasse Qt den neuen Block einfgen
        QTextBlock prev = textCursor().block().previous();
        if( prev.isValid() )
        {
            const int ws = _firstNwsPos( prev );
            textCursor().insertText( prev.text().left( ws - prev.position() ) );
        }
        textCursor().endEditBlock();
        return;
    }
    QPlainTextEdit::keyPressEvent( e );
}

static inline int _indents( const QTextBlock& b )
{
    const QString text = b.text();
    int spaces = 0;
    for( int i = 0; i < text.size(); i++ )
    {
        if( text[i] == QChar('\t') )
            spaces += s_charPerTab;
        else if( text[i] == QChar( ' ' ) )
            spaces++;
        else
            break;
    }
    return spaces / s_charPerTab;
}

void Editor::paintIndents(QPaintEvent *)
{
    QPainter p( viewport() );

    QPointF offset(contentOffset());
    QTextBlock block = firstVisibleBlock();
    const QRect viewportRect = viewport()->rect();
    // erst ab Qt5 const int margin = document()->documentMargin();
    const int margin = 4; // RISK: empirisch ermittelt; unklar, wo der herkommt (blockBoundingRect offensichtlich nicht)

    while( block.isValid() )
    {
        const QRectF r = blockBoundingRect(block).translated(offset);
        p.setPen( Qt::lightGray );
        const int indents = _indents( block );
        for( int i = 0; i <= indents; i++ )
        {
            const int x0 = r.x() + ( i - 1 ) * tabStopWidth() + margin + 1; // + 1 damit Cursor nicht verdeckt wird
            p.drawLine( x0, r.top(), x0, r.bottom() - 1 );
        }
        offset.ry() += r.height();
        if (offset.y() > viewportRect.height())
            break;
        block = block.next();
    }
}

void Editor::updateTabWidth()
{
    setTabStopWidth( fontMetrics().width( QLatin1Char('0') ) * s_charPerTab );
}

void Editor::highlightCurrentLine()
{
    QList<QTextEdit::ExtraSelection> extraSelections;

    QTextEdit::ExtraSelection selection;

    QColor lineColor = QColor(Qt::yellow).lighter(160);

    selection.format.setBackground(lineColor);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);

    setExtraSelections(extraSelections);
}

void Editor::paintHandleArea(QPaintEvent *event)
{
    QPainter painter(d_numberArea);
    painter.fillRect(event->rect(), QColor(224,224,224) );

    QTextBlock block = firstVisibleBlock();
    int blockNumber = block.blockNumber();
    int top = (int) blockBoundingGeometry(block).translated(contentOffset()).top();
    int bottom = top + (int) blockBoundingRect(block).height();

    const int w = fontMetrics().width('w') + 2;
    const int h = fontMetrics().height();
    while (block.isValid() && top <= event->rect().bottom())
    {
        painter.setPen(Qt::black);
        if( d_breakPoints.contains( blockNumber ) )
        {
            const QRect r = QRect( 0, top, d_numberArea->width(), h );
            painter.fillRect( r, Qt::darkRed );
            painter.setPen(Qt::white);
        }
        if( d_showNumbers && block.isVisible() && bottom >= event->rect().top())
        {
            QString number = QString::number(blockNumber + 1);
            painter.drawText(0, top, d_numberArea->width() - 2, h, Qt::AlignRight, number);
        }
        if( blockNumber == d_curPos )
        {
            const QRect r = QRect( d_numberArea->width() - w, top, w, h );
            painter.setBrush(Qt::yellow);
            painter.setPen(Qt::black);
            painter.drawPolygon( QPolygon() << r.topLeft() << r.bottomLeft() << r.adjusted(0,0,0,-h/2).bottomRight() );
        }

        block = block.next();
        top = bottom;
        bottom = top + (int) blockBoundingRect(block).height();
        ++blockNumber;
    }
}

void Editor::selectToMatchingBrace()
{
}

void Editor::indent()
{
    QTextCursor cur = textCursor();

    const int selStart = cur.selectionStart();
    const int selEnd = cur.selectionEnd();
    Q_ASSERT( selStart <= selEnd );

    QTextBlock b = document()->findBlock( selStart );
    cur.beginEditBlock();
    do
    {
        cur.setPosition( b.position() );
        cur.insertText( QChar('\t') );
        b = b.next();
    }while( b.isValid() && b.position() < selEnd );
    cur.endEditBlock();
}

static inline int _indentToPos( const QTextBlock& b, int indent )
{
    const QString text = b.text();
    int spaces = 0;
    for( int i = 0; i < text.size(); i++ )
    {
        if( text[i] == QChar('\t') )
            spaces += s_charPerTab;
        else if( text[i] == QChar( ' ' ) )
            spaces++;
        else
            return b.position() + i; // ohne das aktuelle Zeichen, das ja kein Space ist
        if( ( spaces / s_charPerTab ) >= indent )
            return b.position() + i + 1; // inkl. dem aktuellen Zeichen, mit dem die Bedingung erfllt ist.
    }
    return b.position();
}

void Editor::unindent()
{
    QTextCursor cur = textCursor();

    const int selStart = cur.selectionStart();
    const int selEnd = cur.selectionEnd();
    Q_ASSERT( selStart <= selEnd );

    QTextBlock b = document()->findBlock( selStart );
    cur.beginEditBlock();
    do
    {
        const int indent = _indents(b);
        if( indent > 0 )
        {
            cur.setPosition( b.position() );
            cur.setPosition( _indentToPos( b, indent ), QTextCursor::KeepAnchor );
            cur.insertText( QString( indent - 1, QChar('\t') ) );
        }
        b = b.next();
    }while( b.isValid() && b.position() < selEnd );
    cur.endEditBlock();
}

void Editor::setIndentation(int i)
{
    QTextCursor cur = textCursor();

    const int selStart = cur.selectionStart();
    const int selEnd = cur.selectionEnd();
    Q_ASSERT( selStart <= selEnd );

    QTextBlock b = document()->findBlock( selStart );
    cur.beginEditBlock();
    do
    {
        const int indent = _indents(b);
        cur.setPosition( b.position() );
        cur.setPosition( _indentToPos( b, indent ), QTextCursor::KeepAnchor );
        cur.insertText( QString( i, QChar('\t') ) );
        b = b.next();
    }while( b.isValid() && b.position() < selEnd );
    cur.endEditBlock();
}

void Editor::setShowNumbers(bool on)
{
    d_showNumbers = on;
    updateLineNumberAreaWidth();
    viewport()->update();
}

bool Editor::loadFromFile(const QString &filename)
{
    QFile file(filename);
    if( !file.open(QIODevice::ReadOnly ) )
        return false;
	setText( QString::fromLatin1( file.readAll() ) );
	// TODO: laut Gnat sind Ada-Sourcen in Latin-1; unklar, was man sonst macht.
    document()->setModified( false );
	emit updateCaption(filename);
	return true;
}

bool Editor::loadFromString(const QString &source)
{
	setText( source );
	document()->setModified( false );
	return true;
}

void Editor::addBreakPoint(int l)
{
    d_breakPoints.insert( l );
    d_numberArea->update();
}

void Editor::removeBreakPoint(int l)
{
    d_breakPoints.remove( l );
    d_numberArea->update();
}

void Editor::clearBreakPoints()
{
    d_breakPoints.clear();
	d_numberArea->update();
}

void Editor::installDefaultPopup()
{
	Gui2::AutoMenu* pop = new Gui2::AutoMenu( this, true );
	pop->addCommand( "Open...", this, SLOT(handleOpen()), tr("CTRL+O"), true );
//	pop->addCommand( "Undo", this, SLOT(handleEditUndo()), tr("CTRL+Z"), true );
//	pop->addCommand( "Redo", this, SLOT(handleEditRedo()), tr("CTRL+Y"), true );
	pop->addSeparator();
	//pop->addCommand( "Cut", this, SLOT(handleEditCut()), tr("CTRL+X"), true );
	pop->addCommand( "Copy", this, SLOT(handleEditCopy()), tr("CTRL+C"), true );
	//pop->addCommand( "Paste", this, SLOT(handleEditPaste()), tr("CTRL+V"), true );
	pop->addCommand( "Select all", this, SLOT(handleEditSelectAll()), tr("CTRL+A"), true  );
	//pop->addCommand( "Select Matching Brace", this, SLOT(handleSelectBrace()) );
	pop->addSeparator();
	pop->addCommand( "Find...", this, SLOT(handleFind()), tr("CTRL+F"), true );
	pop->addCommand( "Find again", this, SLOT(handleFindAgain()), tr("F3"), true );
	//pop->addCommand( "Replace...", this, SLOT(handleReplace()), tr("CTRL+R"), true );
	pop->addCommand( "&Goto...", this, SLOT(handleGoto()), tr("CTRL+G"), true );
	pop->addCommand( "Show &Linenumbers", this, SLOT(handleShowLinenumbers()) );
//	pop->addSeparator();
//	pop->addCommand( "Indent", this, SLOT(handleIndent()) );
//	pop->addCommand( "Unindent", this, SLOT(handleUnindent()) );
//	pop->addCommand( "Set Indentation Level...", this, SLOT(handleSetIndent()) );
	pop->addSeparator();
	pop->addCommand( "Print...", this, SLOT(handlePrint()), tr("CTRL+P"), true );
	pop->addCommand( "Export PDF...", this, SLOT(handleExportPdf()), tr("CTRL+SHIFT+P"), true );
	pop->addSeparator();
	pop->addCommand( "Set &Font...", this, SLOT(handleSetFont()) );
	pop->addSeparator();
	pop->addAction(tr("Quit"), qApp, SLOT(quit()) );
	connect( new QShortcut(tr("CTRL+Q"), this ), SIGNAL(activated()), qApp,SLOT(quit() ) );
}

