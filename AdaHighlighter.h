#ifndef ADAHIGHLIGHTER_H
#define ADAHIGHLIGHTER_H

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

#include <QSyntaxHighlighter>

namespace Ada
{
	class Lexer;

	class Highlighter : public QSyntaxHighlighter
	{
	public:
		enum { TokenProp = QTextFormat::UserProperty };
		explicit Highlighter(QTextDocument *parent = 0);
		static QString formatTokenType( quint8 );
	protected:
		// Override
		void highlightBlock( const QString & text );
	private:
		Lexer* d_lex;
		QTextCharFormat d_commentFormat;
		QTextCharFormat d_stringFormat;
		QTextCharFormat d_charFormat;
		QTextCharFormat d_numberFormat;
		QTextCharFormat d_delimiterFormat;
		QTextCharFormat d_keyWordFormat;
		QTextCharFormat d_identFormat;
		QTextCharFormat d_attrFormat;
		QTextCharFormat d_invalidFormat;
	};
}

#endif // ADAHIGHLIGHTER_H
