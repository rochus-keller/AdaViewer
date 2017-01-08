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

#include "AdaHighlighter.h"
#include "AdaLexer.h"
#include <QTextStream>
using namespace Ada;

Highlighter::Highlighter(QTextDocument *parent) :
	QSyntaxHighlighter(parent)
{
	d_lex = new Lexer(this);

	d_commentFormat.setForeground(Qt::darkGreen);
	d_stringFormat.setForeground(Qt::darkRed);
	d_charFormat = d_stringFormat;
	d_charFormat.setFontWeight(QFont::Bold);
	d_numberFormat.setForeground(Qt::red);
	d_delimiterFormat.setForeground(QColor(Qt::darkBlue).lighter(140)); // Qt::darkYellow);
	d_delimiterFormat.setFontWeight(QFont::Bold);
	d_keyWordFormat.setForeground(Qt::darkBlue); // QColor(0x00,0x00,0x7f)); // dunkelblau
	d_keyWordFormat.setFontWeight(QFont::Bold);
	d_identFormat.setForeground(Qt::black);
	d_attrFormat.setForeground(Qt::darkCyan); // braun QColor(128,64,0)
	d_invalidFormat.setForeground( Qt::magenta );
	d_invalidFormat.setUnderlineColor( Qt::red );
	d_invalidFormat.setUnderlineStyle( QTextCharFormat::WaveUnderline );
}

QString Highlighter::formatTokenType(quint8 t)
{
	return QString::fromAscii( Lexer::tokenName( t ) );
}

void Highlighter::highlightBlock(const QString &text)
{
	QTextStream in( const_cast<QString*>( &text ), QIODevice::ReadOnly );
	d_lex->setStream( &in );
	Lexer::Token t = d_lex->nextToken();
	while( !t.isEof() )
	{
		QTextCharFormat f;
		if( t.isComment() )
			f = d_commentFormat;
		else if( t.isString() ) // d_type == Lexer::T_String )
			f = d_stringFormat;
		else if( t.d_type == Lexer::T_Character )
			f = d_charFormat;
		else if( t.isNumber() )
			f = d_numberFormat;
		else if( t.isDelimiter() )
			f = d_delimiterFormat;
		else if( t.isKeyWord() )
			f = d_keyWordFormat;
		else if( t.isIdent() )
			f = d_identFormat;
		else if( t.isAttr() )
			f = d_attrFormat;
		else
			f = d_invalidFormat;
		f.setProperty( TokenProp, t.d_type );
		setFormat( t.d_col, t.d_len, f );
		t = d_lex->nextToken();
	}
	d_lex->setStream(0);
}
