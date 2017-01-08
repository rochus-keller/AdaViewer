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

#include "AdaLexer.h"
#include <QIODevice>
#include <QTextStream>
#include <QtDebug>
using namespace Ada;

static const char* s_tokenName[] =
{
	"?",
	"abort", "abs", "abstract", "accept", "access", "aliased", "all", "and", "array", "at",
	"begin", "body",
	"case", "constant",
	"declare", "delay", "delta", "digits", "do",
	"else", "elsif", "end", "entry", "exception", "exit",
	"for", "function",
	"generic", "goto",
	"if", "in", "interface", "is",
	"limited", "loop",
	"mod",
	"new", "not", "null",
	"of", "or", "others", "out", "overriding",
	"package", "pragma", "private", "procedure", "protected",
	"raise", "range", "record", "rem", "renames", "requeue", "return", "reverse",
	"select", "separate", "some", "subtype", "synchronized",
	"tagged", "task", "terminate", "then", "type",
	"until", "use",
	"when", "while", "with",
	"xor",
	"Colon", "Comma", "Dot", "Semicolon", "Tick",
	"LParen", "RParen",
	"Ampers",
	"Bar",
	"Eq", "Neq",
	"Lt", "Leq", "Geq", "Gt",
	"Plus", "Minus", "Star", "Slash",
	"Arrow", "Assig", "DoubleDot", "DoubleStar",
	"LLBrack", "RLBrack", "Box",
	"Number",
	"Character", "String",
	"Identifier",
	"Attribute",
	"Comment",
	"EOF",
	0
};

const char *Lexer::tokenName(quint8 type, bool asSymbol)
{
	if( type <= T_EOF )
	{
		if( asSymbol && isDelimiter( type ) )
		{
			switch( type )
			{
			case T_Colon:
				return ":";
			case T_Comma:
				return ",";
			case T_Dot:
				return ".";
			case T_Semicolon:
				return ";";
			case T_Tick:
				return "'";
			case T_LParen:
				return "(";
			case T_RParen:
				return ")";
			case T_Ampers:
				return "&";
			case T_Bar:
				return "|";
			case T_Eq:
				return "=";
			case T_Neq:
				return "/=";
			case T_Lt:
				return "<";
			case T_Leq:
				return "<=";
			case T_Geq:
				return ">=";
			case T_Gt:
				return ">";
			case T_Plus:
				return "+";
			case T_Minus:
				return "-";
			case T_Star:
				return "*";
			case T_Slash:
				return "/";
			case T_Arrow:
				return "=>";
			case T_Assig:
				return ":=";
			case T_DoubleDot:
				return "..";
			case T_DoubleStar:
				return "**";
			case T_LLBrack:
				return "<<";
			case T_RLBrack:
				return ">>";
			case T_Box:
				return "<>";
			default:
				Q_ASSERT( false );
			}
		}
		return s_tokenName[ type ];
	}else
		return "?";
}

Lexer::Lexer(QObject *parent) :
	QObject(parent), d_in(0),d_lineNr(0),d_colNr(0),d_ownsStream(false),d_lastTokenType(T_Invalid)
{
}

Lexer::~Lexer()
{
	if( d_in && d_ownsStream )
		delete d_in;
}

void Lexer::setStream(QTextStream *in, bool haveOwnership )
{
	if( d_in != 0 && d_ownsStream )
		delete d_in;
	d_in = in;
	d_ownsStream = haveOwnership;
	reset();
}

void Lexer::reset()
{
	if( d_in )
	{
		d_in->seek(0);
		d_in->reset();
		d_in->resetStatus();
	}
	d_lineNr = 0;
	d_colNr = 0;
	d_line.clear();
	d_lastTokenType = T_Invalid;
}

Lexer::Token Lexer::nextToken()
{
	if( d_in == 0 )
		return token( T_EOF, 0 );
	skipWhiteSpace();
	while( d_colNr >= d_line.size() )
	{
		if( d_in->atEnd() )
			return token( T_EOF, 0 );
		nextLine();
		skipWhiteSpace();
	}
	Q_ASSERT( d_colNr < d_line.size() );
	while( d_colNr < d_line.size() )
	{
		const QChar ch = d_line[d_colNr];
		const ushort ucs = ch.unicode();
		switch( ucs )
		{
		case '&':
			return token(T_Ampers );
		case '(':
			return token( T_LParen );
		case ')':
			return token( T_RParen );
		case '*':
			if( lookAhead(1) == '*' )
				return token( T_DoubleStar, 2 );
			else
				return token( T_Star );
		case '+':
			return token( T_Plus );
		case ',':
			return token( T_Comma );
		case '-':
			if( lookAhead(1) == '-' )
				return token(T_Comment, d_line.size() - d_colNr, d_line.mid( d_colNr + 2 ) );
			else
				return token(T_Minus);
		case '.':
			if( lookAhead(1) == '.' )
				return token( T_DoubleDot, 2 );
			else
				return token( T_Dot );
		case '/':
			if( lookAhead(1) == '=' )
				return token( T_Neq, 2 );
			else
				return token( T_Slash );
		case ':':
			if( lookAhead(1) == '=' )
				return token( T_Assig, 2 );
			else
				return token( T_Colon );
		case ';':
			return token( T_Semicolon );
		case '<':
			switch( lookAhead(1) )
			{
			case '=':
				return token( T_Leq, 2 );
			case '<':
				return token( T_LLBrack, 2 );
			case '>':
				return token( T_Box, 2 );
			default:
				return token( T_Lt );
			}
		case '=':
			if( lookAhead(1) == '>' )
				return token( T_Arrow, 2 );
			else
				return token( T_Eq );
		case '>':
			switch( lookAhead(1) )
			{
			case '=':
				return token( T_Geq, 2 );
			case '>':
				return token( T_RLBrack, 2 );
			default:
				return token( T_Gt );
			}
		case '|':
			return token( T_Bar);
		case '\'':
			if( lookAhead(2) == '\'' )
				return token( T_Character, 3, d_line.mid( d_colNr + 1, 1 ) );
			else
				return token( T_Tick );
		case '"':
			return string();
		default:
			break;
		}
		if( ch.isDigit() )
		{
			// Numeric Literal
			return numeric();
		}else if( ch.isLetter() )
		{
			// Identifier oder Reserved Word
			return ident();
		}else
		{
			// Error
			return token( T_Invalid, 1, tr("unexpected character '%1'").arg(ch) );
		}
	}
	Q_ASSERT( false );
	return T_Invalid;
}

bool Lexer::isAda83KeyWord(quint8 type)
{
	switch( type )
	{
	case T_abort:
	case T_declare:
	case T_generic:
	case T_of:
	case T_select:
	case T_abs:
	case T_delay:
	case T_goto:
	case T_or:
	case T_separate:
	case T_accept:
	case T_delta:
	case T_others:
	case T_subtype:
	case T_access:
	case T_digits:
	case T_if:
	case T_out:
	case T_all:
	case T_do:
	case T_in:
	case T_task:
	case T_and:
	case T_is:
	case T_package:
	case T_terminate:
	case T_array:
	case T_pragma:
	case T_then:
	case T_at:
	case T_else:
	case T_private:
	case T_type:
	case T_elsif:
	case T_limited:
	case T_procedure:
	case T_end:
	case T_loop:
	case T_begin:
	case T_entry:
	case T_raise:
	case T_use:
	case T_body:
	case T_exception:
	case T_range:
	case T_exit:
	case T_mod:
	case T_record:
	case T_when:
	case T_rem:
	case T_while:
	case T_new:
	case T_renames:
	case T_with:
	case T_case:
	case T_for:
	case T_not:
	case T_return:
	case T_constant:
	case T_function:
	case T_null:
	case T_reverse:
	case T_xor:
		return true;
	default:
		return false;
	}
}

bool Lexer::isAda95KeyWord(quint8 type)
{
	switch( type )
	{
	case T_abstract:
	case T_aliased:
	case T_protected:
	case T_requeue:
	case T_tagged:
	case T_until:
		return true;
	default:
		return isAda83KeyWord(type);
	}
}

bool Lexer::isAda05KeyWord(quint8 type)
{
	switch( type )
	{
	case T_interface:
	case T_overriding:
	case T_synchronized:
		return true;
	default:
		return isAda95KeyWord(type);
	}
}

bool Lexer::isAda12KeyWord(quint8 type)
{
	if( type == T_some )
		return true;
	else
		return isAda05KeyWord(type);
}

bool Lexer::isKeyWord(quint8 type)
{
	return type >= T_abort && type <= T_xor;
}

bool Lexer::isDelimiter(quint8 type)
{
	return type >= T_Colon && type <= T_Box;
}

bool Lexer::isNumber(quint8 type)
{
	return type == T_Number;
}

void Lexer::nextLine()
{
	d_colNr = 0;
	d_lineNr++;
	d_line = d_in->readLine();
}

void Lexer::skipWhiteSpace()
{
	while( d_colNr < d_line.size() && d_line[d_colNr].isSpace() )
		d_colNr++;
}

char Lexer::lookAhead(quint32 off) const
{
	if( int( d_colNr + off ) < d_line.size() )
	{
		const QChar ch = d_line[ d_colNr + off ];
		if( ch.unicode() < 0xff )
			return ch.unicode();
		else
			return 0;
	}else
		return 0;
}

QChar Lexer::lookAhead2(quint32 off) const
{
	if( int( d_colNr + off ) < d_line.size() )
		return d_line[ d_colNr + off ];
	else
		return QChar();
}

Lexer::Token Lexer::token(Lexer::TokenType tt, int len, const QString &val)
{
	Token t( tt, d_lineNr, d_colNr, len, val );
	d_colNr += len;
	d_lastTokenType = tt;
	return t;
}

Lexer::Token Lexer::string()
{
	int off = 1;
	while(true)
	{
		const QChar ch = lookAhead2(off);
		if( ch.unicode() == '"' )
		{
			if( lookAhead(off + 1) == '"' )
			{
				off += 2;
			}else
			{
				break; // End of String
			}
		}else if( ch.isPrint() )
			off++;
		else if( ch.isNull() )
			return token( T_Invalid, d_line.size() - d_colNr, tr("non terminated string") );
	}
	const QString str = d_line.mid( d_colNr + 1, off - 1 ).replace( "\"\"", "\"" );
	return token( T_String, off + 1, str );
}

Lexer::Token Lexer::ident()
{
	// Hier wurde bereits geprft, dass lookAhead2(0).isLetter() gilt
	int off = 0;
	while( true )
	{
		const QChar ch = lookAhead2(off);
		if( !ch.isLetterOrNumber() && ch.unicode() != '_' )
			break;
		else
			off++;
	}
	const QString str = d_line.mid(d_colNr, off );
	const TokenType tt = findReservedWord( str );
	if( d_lastTokenType == T_Tick )
		return token( T_Attribute, off, str );
	else if( tt != T_Invalid )
		return token( tt, off ); // TODO: Ada-Version prfen
	else
		return token( T_Identifier, off, str );
}

Lexer::Token Lexer::numeric()
{
	// qDebug() << "AdaLexer::numeric" << d_colNr << d_line.mid(d_colNr);

	// Hier wurde bereits geprft, dass lookAhead2(0).isDigit() gilt
	NumberParser np( d_line, d_colNr );
	if( !np.parse() )
	{
		return token( T_Invalid, np.getOff(), tr( np.getError() ) );
	}
	return token( T_Number, np.getOff(), d_line.mid( d_colNr, np.getOff() ) );
}

static inline ushort at( const QString& str, int off )
{
	if( off >= 0 && off < str.size() )
		return str[off].unicode();
	else
		return 0;
}

static inline bool check( const QString& str, int off, const char* match )
{
	// qDebug() << str << off << match;
	int i = 0;
	for( i = off; i < str.length(); i++ )
	{
		if( str[i].toLower().unicode() != match[i - off] )
			return false;
	}
	if( match[i - off] != 0 )
		return false;
	// Macht Kopie: return str.mid( off ).toLower().toAscii() == match;
	return true;
}

Lexer::TokenType Lexer::findReservedWord(const QString & str)
{
	switch( at(str,0) )
	{
	case 'a':
	case 'A':
		switch( at(str,1) )
		{
		case 'b':
		case 'B':
			switch( at(str,2) )
			{
			case 'o':
			case 'O':
				if( check( str, 3, "rt" ) )
					return T_abort;
				break;
			case 's':
			case 'S':
				if( at(str,3) == 0 )
					return T_abs;
				else if( check( str, 3, "tract" ) )
					return T_abstract;
				break;
			default:
				break;
			}
			break;
		case 'c':
		case 'C':
			if( check( str, 2, "cept" ) )
				return T_accept;
			else if( check( str, 2, "cess" ) )
				return T_access;
			break;
		case 'l':
		case 'L':
			if( check( str, 2, "iased" ) )
				return T_aliased;
			else if( check( str, 2, "l" ) )
				return T_all;
			break;
		case 'n':
		case 'N':
			if( check( str, 2, "d" ) )
				return T_and;
			break;
		case 'r':
		case 'R':
			if( check( str, 2, "ray" ) )
				return T_array;
			break;
		case 't':
		case 'T':
			if( at(str,3) == 0 )
				return T_at;
			break;
		default:
			break;
		}
		break;
	case 'b':
	case 'B':
		switch( at(str,1) )
		{
		case 'e':
		case 'E':
			if( check( str, 2, "gin" ) )
				return T_begin;
			break;
		case 'o':
		case 'O':
			if( check( str, 2, "dy" ) )
				return T_body;
			break;
		default:
			break;
		}
		break;
	case 'c':
	case 'C':
		switch( at(str,1) )
		{
		case 'a':
		case 'A':
			if( check( str, 2, "se" ) )
				return T_case;
			break;
		case 'o':
		case 'O':
			if( check( str, 2, "nstant" ) )
				return T_constant;
			break;
		default:
			break;
		}
		break;
	case 'd':
	case 'D':
		switch( at(str,1) )
		{
		case 'e':
		case 'E':
			if( check( str, 2, "clare" ) )
				return T_declare;
			else if( check( str, 2, "lay" ) )
				return T_delay;
			else if( check( str, 2, "lta" ) )
				return T_delta;
			break;
		case 'i':
		case 'I':
			if( check( str, 2, "gits" ) )
				return T_digits;
			break;
		case 'o':
		case 'O':
			if( check( str, 2, "" ) )
				return T_do;
			break;
		default:
			break;
		}
		break;
	case 'e':
	case 'E':
		switch( at(str,1) )
		{
		case 'l':
		case 'L':
			if( check( str, 2, "se" ) )
				return T_else;
			else if( check( str, 2, "sif" ) )
				return T_elsif;
			break;
		case 'n':
		case 'N':
			if( check( str, 2, "d" ) )
				return T_end;
			else if( check( str, 2, "try" ) )
				return T_entry;
			break;
		case 'x':
		case 'X':
			if( check( str, 2, "ception" ) )
				return T_exception;
			else if( check( str, 2, "it" ) )
				return T_exit;
			break;
		default:
			break;
		}
		break;
	case 'f':
	case 'F':
		if( check( str, 1, "or" ) )
			return T_for;
		else if( check( str, 1, "unction" ) )
			return T_function;
		break;
	case 'g':
	case 'G':
		if( check( str, 1, "eneric" ) )
			return T_generic;
		else if( check( str, 1, "oto" ) )
			return T_goto;
		break;
	case 'i':
	case 'I':
		switch( at(str,1) )
		{
		case 'f':
		case 'F':
			if( check( str, 2, "" ) )
				return T_if;
			break;
		case 'n':
		case 'N':
			if( check( str, 2, "terface" ) )
				return T_interface;
			else if ( check( str, 2, "" ) )
				return T_in;
			break;
		case 's':
		case 'S':
			if( check( str, 2, "" ) )
				return T_is;
			break;
		default:
			break;
		}
		break;
	case 'l':
	case 'L':
		if( check( str, 1, "imited" ) )
			return T_limited;
		else if( check( str, 1, "oop" ) )
			return T_loop;
		break;
	case 'm':
	case 'M':
		if( check( str, 1, "od" ) )
			return T_mod;
		break;
	case 'n':
	case 'N':
		if( check( str, 1, "ew" ) )
			return T_new;
		else if( check( str, 1, "ot" ) )
			return T_not;
		else if( check( str, 1, "ull" ) )
			return T_null;
		break;
	case 'o':
	case 'O':
		if( check( str, 1, "f" ) )
			return T_of;
		else if( check( str, 1, "r" ) )
			return T_or;
		else if( check( str, 1, "thers" ) )
			return T_others;
		else if( check( str, 1, "ut" ) )
			return T_out;
		else if( check( str, 1, "verriding" ) )
			return T_overriding;
		break;
	case 'p':
	case 'P':
		switch( at(str,1) )
		{
		case 'a':
		case 'A':
			if( check( str, 2, "ckage" ) )
				return T_package;
			break;
		case 'r':
		case 'R':
			if( check( str, 2, "agma" ) )
				return T_pragma;
			else if ( check( str, 2, "ivate" ) )
				return T_private;
			else if ( check( str, 2, "ocedure" ) )
				return T_procedure;
			else if ( check( str, 2, "otected" ) )
				return T_protected;
			break;
		default:
			break;
		}
		break;
	case 'r':
	case 'R':
		switch( at(str,1) )
		{
		case 'a':
		case 'A':
			if( check( str, 2, "ise" ) )
				return T_raise;
			else if ( check( str, 2, "nge" ) )
				return T_range;
			break;
		case 'e':
		case 'E':
			if( check( str, 2, "cord" ) )
				return T_record;
			else if ( check( str, 2, "m" ) )
				return T_rem;
			else if ( check( str, 2, "names" ) )
				return T_renames;
			else if ( check( str, 2, "queue" ) )
				return T_requeue;
			else if ( check( str, 2, "turn" ) )
				return T_return;
			else if ( check( str, 2, "verse" ) )
				return T_reverse;
			break;
		default:
			break;
		}
		break;
	case 's':
	case 'S':
		switch( at(str,1) )
		{
		case 'e':
		case 'E':
			if( check( str, 2, "lect" ) )
				return T_select;
			else if ( check( str, 2, "parate" ) )
				return T_separate;
			break;
		case 'o':
		case 'O':
			if( check( str, 2, "me" ) )
				return T_some;
			break;
		case 'u':
		case 'U':
			if( check( str, 2, "btype" ) )
				return T_subtype;
			break;
		case 'y':
		case 'Y':
			if( check( str, 2, "nchronized" ) )
				return T_synchronized;
			break;
		default:
			break;
		}
		break;
	case 't':
	case 'T':
		switch( at(str,1) )
		{
		case 'a':
		case 'A':
			if( check( str, 2, "gged" ) )
				return T_tagged;
			else if ( check( str, 2, "sk" ) )
				return T_task;
			break;
		case 'e':
		case 'E':
			if( check( str, 2, "rminate" ) )
				return T_terminate;
			break;
		case 'h':
		case 'H':
			if( check( str, 2, "en" ) )
				return T_then;
			break;
		case 'y':
		case 'Y':
			if( check( str, 2, "pe" ) )
				return T_type;
			break;
		default:
			break;
		}
		break;
	case 'u':
	case 'U':
		if( check( str, 1, "ntil" ) )
			return T_until;
		else if( check( str, 1, "se" ) )
			return T_use;
		break;
	case 'w':
	case 'W':
		switch( at(str,1) )
		{
		case 'h':
		case 'H':
			if( check( str, 2, "en" ) )
				return T_when;
			else if( check( str, 2, "ile" ) )
				return T_while;
			break;
		case 'i':
		case 'I':
			if( check( str, 2, "th" ) )
				return T_with;
			break;
		default:
			break;
		}
		break;
	case 'x':
	case 'X':
		if( check( str, 1, "or" ) )
			return T_xor;
		break;
	default:
		break;
	}
	return T_Invalid;
}

NumberParser::NumberParser(const QString & str, int start):
	d_str(str),d_start(start),d_off(start),d_hasDecimals(false),
	d_hasExponent(false),d_isBased(false)
{
}

bool NumberParser::parse()
{
	// decimal_literal ::= numeral [.numeral] [exponent]
	// based_literal ::= base # based_numeral [.based_numeral] # [exponent]
	// base ::= numeral

	d_off = 0;
	d_hasDecimals = false;
	d_hasExponent = false;
	d_isBased = false;
	if( !numeral() )
		return false;
	if( lookAhead( d_off ) == QLatin1Char('#') )
	{
		// The base (the numeric value of the decimal numeral preceding the first #) shall be at least two and at most sixteen.
		// The conventional meaning of based notation is assumed. An exponent indicates the power of the base by which the value of
		// the based_literal without the exponent is to be multiplied to obtain the value of the based_literal with the exponent.
		// The base and the exponent, if any, are in decimal notation.
		d_isBased = true;
		d_off++;
		if( !basedNumeral() )
			return false;
		if( lookAhead( d_off ) == QLatin1Char('.') )
		{
			d_hasDecimals = true;
			d_off++;
			if( !basedNumeral() )
				return false;
		}
		if( lookAhead( d_off ) != QLatin1Char('#') )
			return error( "expecting #" );
		d_off++;

		if( lookAhead( d_off ).toLower() == QLatin1Char('e') )
		{
			d_hasExponent = true;
			d_off++;
			if( !exponent() )
				return false;
		}
	}else
	{
		if( lookAhead( d_off ) == QLatin1Char('.') )
		{
			d_hasDecimals = true;
			d_off++;
			if( !numeral() )
				return false;
		}
		if( lookAhead( d_off ).toLower() == QLatin1Char('e') )
		{
			d_hasExponent = true;
			d_off++;
			if( !exponent() )
				return false;
		}
	}
	return true;
}

bool NumberParser::numeral()
{
	// d_cur steht auf erster Zahl

	// numeral ::= digit {[underline] digit}
	// digit ::= 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9
	// An underline character in a numeric_literal does not affect its meaning.

	QChar ch = lookAhead( d_off );
	if( !ch.isDigit() )
		return error( "expecting digit" );
	d_off++;
	ch = lookAhead( d_off );
	while( true )
	{
		if( ch == QLatin1Char('_') || ch.isDigit() )
		{
			if( ch == QLatin1Char('_') )
			{
				d_off++;
				ch = lookAhead( d_off );
				if( ch.isDigit() )
				{
					d_off++;
					ch = lookAhead( d_off );
				}else
					return error( "expecting digit" );
			}else if( ch.isDigit() )
			{
				d_off++;
				ch = lookAhead( d_off );
			}
		}else
			break;
	}
	return true;
}

static bool extended_digit( const QChar& ch )
{
	if( ch.isDigit() )
		return true;
	switch( ch.unicode() )
	{
	case 'a':
	case 'A':
	case 'b':
	case 'B':
	case 'c':
	case 'C':
	case 'd':
	case 'D':
	case 'e':
	case 'E':
	case 'f':
	case 'F':
		return true;
	default:
		return false;
	}
}

bool NumberParser::basedNumeral()
{
	// based_numeral ::= extended_digit {[underline] extended_digit}
	// extended_digit ::= digit | A | B | C | D | E | F
	// The extended_digits A through F represent the digits ten through fifteen, respectively.
	// The value of each extended_digit of a based_literal shall be less than the base.
	// The extended_digits A through F can be written either in lower case or in upper case, with the same meaning.

	QChar ch = lookAhead( d_off );
	if( !extended_digit(ch) )
		return error( "expecting extended digit" );
	d_off++;
	ch = lookAhead( d_off );
	while( true )
	{
		if( ch == QLatin1Char('_') || extended_digit(ch) )
		{
			if( ch == QLatin1Char('_') )
			{
				d_off++;
				ch = lookAhead( d_off );
				if( extended_digit(ch) )
				{
					d_off++;
					ch = lookAhead( d_off );
				}else
					return error( "expecting extended digit" );
			}else if( extended_digit(ch) )
			{
				d_off++;
				ch = lookAhead( d_off );
			}
		}else
			break;
	}
	return true;
}

bool NumberParser::exponent()
{
	// E wurde bereits gelesen
	// d_cur steht auf +, - oder Zahl

	// exponent ::= E [+] numeral | E - numeral
	// An exponent for an integer literal shall not have a minus sign.
	// The letter E of an exponent can be written either in lower case or in upper case, with the same meaning.
	// An exponent indicates the power of ten by which the value of the decimal_literal
	// without the exponent is to be multiplied to obtain the value of the decimal_literal with the exponent.

	QChar ch = lookAhead( d_off );
	if( ch == QLatin1Char('+') || ch == QLatin1Char('-') || ch.isDigit() )
	{
		if( ch == QLatin1Char('+') || ch == QLatin1Char('-') )
		{
			d_off++;
			ch = lookAhead( d_off );
			if( !numeral() )
				return false;
		}else if( ch.isDigit() )
		{
			if( !numeral() )
				return false;
		}
	}else
		return error( "expecting plus, minus or digit" );
	return true;
}

bool NumberParser::error(const char *str)
{
	d_error = str;
	return false;
}

QChar NumberParser::lookAhead(quint32 off) const
{
	if( int( d_start + off ) < d_str.size() )
		return d_str[ d_start + off ];
	else
		return QChar();
}



