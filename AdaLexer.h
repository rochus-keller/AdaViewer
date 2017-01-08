#ifndef ADALEXER_H
#define ADALEXER_H

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

#include <QObject>
#include <QVariant>

class QTextStream;

namespace Ada
{
	class Lexer : public QObject
	{
	public:
		enum TokenType { // Ada 2012
			T_Invalid,
			T_abort, T_abs, T_abstract, T_accept, T_access, T_aliased, T_all, T_and, T_array, T_at,
			T_begin, T_body,
			T_case, T_constant,
			T_declare, T_delay, T_delta, T_digits, T_do,
			T_else, T_elsif, T_end, T_entry, T_exception, T_exit,
			T_for, T_function,
			T_generic, T_goto,
			T_if, T_in, T_interface, T_is,
			T_limited, T_loop,
			T_mod,
			T_new, T_not, T_null,
			T_of, T_or, T_others, T_out, T_overriding,
			T_package, T_pragma, T_private, T_procedure, T_protected,
			T_raise, T_range, T_record, T_rem, T_renames, T_requeue, T_return, T_reverse,
			T_select, T_separate, T_some, T_subtype, T_synchronized,
			T_tagged, T_task, T_terminate, T_then, T_type,
			T_until, T_use,
			T_when, T_while, T_with,
			T_xor,
			T_Colon, T_Comma, T_Dot, T_Semicolon, T_Tick,         // : , . ; '
			T_LParen, T_RParen,                                   // ( )
			T_Ampers,                                             // & (concatenate)
			T_Bar,                                                // | (alternative)
			T_Eq, T_Neq,                                          // = /=
			T_Lt, T_Leq, T_Geq, T_Gt,                             // < <= >= >
			T_Plus, T_Minus, T_Star, T_Slash,                     // + - * / (times, divide)
			T_Arrow, T_Assig, T_DoubleDot, T_DoubleStar,          // => := .. **
			T_LLBrack, T_RLBrack, T_Box,                          // << >> <> (left/right label bracket
			T_Number,
			T_Character, T_String,  // 'a' "xyz"
			T_Identifier,
			T_Attribute,
			T_Comment,              // -- to EOL
			T_EOF
		};
		struct Token
		{
			quint8 d_type;
			quint16 d_line;
			quint16 d_col, d_len;
			QString d_val;
			Token(TokenType t = T_EOF, quint32 line = 0, quint16 col = 0, quint16 len = 0, const QString& val = QString() ):
				d_type(t),d_line(line),d_col(col),d_len(len),d_val(val){}
			bool isValid() const { return d_type != T_EOF && d_type != T_Invalid; }
			bool isEof() const { return d_type == T_EOF; }
			const char* getName() const { return Lexer::tokenName( d_type ); }
			bool isKeyWord() const { return Lexer::isKeyWord( d_type ); }
			bool isDelimiter() const { return Lexer::isDelimiter( d_type ); }
			bool isNumber() const { return Lexer::isNumber( d_type ); }
			bool isString() const { return d_type == T_String || d_type == T_Character; }
			bool isIdent() const { return d_type == T_Identifier; }
			bool isAttr() const { return d_type == T_Attribute; }
			bool isComment() const { return d_type == T_Comment; }
		};

		explicit Lexer(QObject *parent = 0);
		~Lexer();

		void setStream( QTextStream* in, bool haveOwnership = false );
		void reset();
		Token nextToken();

		static bool isAda83KeyWord( quint8 type );
		static bool isAda95KeyWord( quint8 type );
		static bool isAda05KeyWord( quint8 type );
		static bool isAda12KeyWord( quint8 type );
		static bool isKeyWord( quint8 type );
		static bool isDelimiter( quint8 type );
		static bool isNumber( quint8 type );
		static const char* tokenName( quint8 type, bool asSymbol = false );
		static TokenType findReservedWord( const QString& );
	protected:
		void nextLine();
		void skipWhiteSpace();
		char lookAhead( quint32 ) const;
		QChar lookAhead2( quint32 ) const;
		Token token( TokenType, int len = 1, const QString& val = QString() );
		Token string();
		Token ident();
		Token numeric();
	private:
		QTextStream* d_in;
		quint16 d_lineNr; // current line, starting with 1
		quint16 d_colNr;  // current column (left of char), starting with 0
		QString d_line;
		quint8 d_lastTokenType;
		bool d_ownsStream;
	};

	class NumberParser
	{
	public:
		NumberParser( const QString&, int start );
		const QByteArray& getError() const { return d_error; }
		bool parse();
		int getOff() const { return d_off; }
		bool hasDecimals() const { return d_hasDecimals; }
		bool hasExponent() const { return d_hasExponent; }
		bool isBased() const { return d_isBased; }
	protected:
		QChar lookAhead( quint32 ) const;
		bool numeral();
		bool basedNumeral();
		bool exponent();
		bool error( const char* );
	private:
		QByteArray d_error;
		const QString d_str;
		const int d_start;
		int d_off;
		bool d_hasDecimals;
		bool d_hasExponent;
		bool d_isBased;
	};
}

#endif // ADALEXER_H
