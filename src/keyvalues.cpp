#include "keyvalues.hpp"

#include <iostream>
#include <algorithm>
#include <sstream>

namespace KV
{
	static std::function< void( const std::string_view &output ) > debugCallback;

	void setDebugCallback( std::function< void( const std::string_view &output ) > callback )
	{
		debugCallback = callback;
	}

	ParseException::LineColumn_t ResolveLineColumn ( const std::string_view &buffer, size_t index )
	{
		constexpr auto UTF8_MB_CONTINUE = 2;

		if ( index == std::string::npos || index >= buffer.size() )
			{};

		size_t line = 1;
		size_t column = 0;

		for ( size_t i = 0; i < index; ++i  )
		{
			const unsigned char &c = buffer[ i ];

			if ( c == '\n' )
			{
				column = 0;
				++line;
			}
			else if ( ( c >> 6 ) != UTF8_MB_CONTINUE && c != '\r' )
				++column;

		}

		return ParseException::LineColumn_t{ line, column };
	};

	ExpressionEngine::ExpressionEngine( bool useAutomaticDefaults /*= true*/ )
	{
		if ( useAutomaticDefaults )
		{
#if _WIN64 || __amd64__
			setCondition( "x64", true );
#else
			setCondition( "x86", true );
#endif

#ifdef _WIN32
			setCondition( "WINDOWS", true );
#endif

#ifdef __linux__
			setCondition( "LINUX", true );
#endif

		}
	}

	void ExpressionEngine::setCondition( const std::string &condition, bool value )
	{
		conditions[ condition ] = value;
	}

	bool ExpressionEngine::getCondition( const std::string &condition ) const
	{
		if ( auto it = conditions.find( condition ); it != conditions.cend() )
			return it->second;

		return false;
	}

	ExpressionEngine::ExpressionResult ExpressionEngine::evaluateExpression( const std::string_view &expression, const size_t offset /*= 0*/ ) const
	{
		constexpr const std::array< char, 11 > controls = { '$', '&', '|', '!', '(', ')', '[', ']', '\n', ' ', '\t' };
		constexpr const std::array< char, 7 > unsupportedOps = { '>', '<', '=', '+', '-', '*', '/' };

		enum class LogicOp
		{
			NONE,
			OR,
			AND,
			UNSET // Special case for error handling when parsing
		};

		auto peekChar = [ &expression ]( const size_t index ) -> char
		{
			return ( index < expression.size() ) ? expression[ index ] : '\0';
		};

		auto evaluate = [ & ]( const size_t start, const char expressionEnd, auto &evaluateRecursive ) -> ExpressionResult
		{
			LogicOp currentOp = LogicOp::NONE;
			std::optional< bool > evaluation;

			bool isNot = false;

			for ( size_t i = start; i < expression.size(); ++i )
			{
				if ( expression[ i ] == '\n' )
				{
					const std::string errMsg = std::string( "Expected '" ) + expressionEnd + std::string( "', got EOL instead" );
					throw ParseException( errMsg, ResolveLineColumn( expression, i ) );
				}
				else if ( expression[ i ] == expressionEnd )
				{
					if ( !evaluation.has_value() )
						throw ParseException( "Expected an expression", ResolveLineColumn( expression, i ) );
					else if ( currentOp != LogicOp::NONE && currentOp != LogicOp::UNSET )
					{
						const std::string errMsg = std::string( "Expected primary-expression before '" ) + expressionEnd + std::string( "' token" );
						throw ParseException( errMsg, ResolveLineColumn( expression, i ) );
					}
					else
						return { evaluation.value(), i };
				}
				else if ( expression[ i ] == '!' )
				{
					isNot = !isNot;
					continue;
				}
				else if ( expression[ i ] == '(' )
				{
					ExpressionResult nextEvaluation = evaluateRecursive( i + 1, ')', evaluateRecursive );

					if ( isNot )
					{
						nextEvaluation.result = !nextEvaluation.result;
						isNot = false;
					}

					switch ( currentOp )
					{
						case LogicOp::NONE:
						{
							evaluation = nextEvaluation.result;
							break;
						}
						case LogicOp::OR:
						{
							evaluation = ( evaluation.value() || nextEvaluation.result );
							break;
						}
						case LogicOp::AND:
						{
							evaluation = ( evaluation.value() && nextEvaluation.result );
							break;
						}
						case LogicOp::UNSET:
						{
							throw ParseException( "Expected logical operator, expression incomplete", ResolveLineColumn( expression, i ) );
							break;
						}
					}

					currentOp = LogicOp::UNSET;
					i = nextEvaluation.end;

					continue;
				}
				else if ( expression[ i ] == '$' )
				{
					size_t len = 0;

					for ( size_t j = i + 1; j < expression.size(); ++j )
					{
						if ( std::find( controls.begin(), controls.end(), expression[ j ] ) != controls.end() )
							break;

						++len;
					}

					if ( len == 0 )
						throw ParseException( "Expected symbol", ResolveLineColumn( expression, i ) );
					else
					{
						const std::string name = std::string( expression, i + 1, len );
						const bool condition = ( isNot ) ? !getCondition( name ) : getCondition( name );

						isNot = false;

						switch ( currentOp )
						{
							case LogicOp::NONE:
							{
								evaluation = condition;
								break;
							}
							case LogicOp::OR:
							{
								evaluation = ( evaluation.value() || condition );
								break;
							}
							case LogicOp::AND:
							{
								evaluation = ( evaluation.value() && condition );
								break;
							}
							case LogicOp::UNSET:
							{
								throw ParseException( "Expected logical operator, expression incomplete", ResolveLineColumn( expression, i ) );
								break;
							}
						}

						currentOp = LogicOp::UNSET;
						i += len;
						continue;
					}

				}
				else if ( expression[ i ] == '&' )
				{
					if ( peekChar( i + 1 ) != '&' )
					{
						throw ParseException( "Bitwise operators not supported", ResolveLineColumn( expression, i ) );
					}
					else
					{
						currentOp = LogicOp::AND;
						++i;
						continue;
					}
				}
				else if ( expression[ i ] == '|' )
				{
					if ( peekChar( i + 1 ) != '|' )
					{
						throw ParseException( "Bitwise operators not supported", ResolveLineColumn( expression, i ) );
					}
					else
					{
						currentOp = LogicOp::OR;
						++i;
						continue;
					}
				}
				else if ( auto it = std::find( unsupportedOps.cbegin(), unsupportedOps.cend(), expression[ i ] ); it != unsupportedOps.cend() )
				{
					const std::string errMsg = std::string( "Unsupported operator '" ) + *it + std::string( "'" ) ;
					throw ParseException( errMsg, ResolveLineColumn( expression, i ) );
				}
			}

			throw ParseException( "Expected end of expression", ResolveLineColumn( expression, offset ) );
		};

		if ( expression[ offset ] != '[' )
			throw ParseException( "Invalid expression", ResolveLineColumn( expression, offset ) );

		return evaluate( offset + 1, ']', evaluate );
	}

	KeyValues &KeyValues::getRoot()
	{
		if ( isRoot() )
			return *this;

		auto root = parentKV;
		while ( root->parentKV != nullptr )
			root = root->parentKV;

		return *root;
	}

	KeyValues &KeyValues::createKey( const std::string_view &name )
	{
		auto it = keyvalues.insert( std::make_pair( std::string( name ), std::make_unique< KeyValues >() ) );
		KeyValues &kv = *it->second;
		kv.key = &it->first;
		kv.parentKV = this;

		if ( !isRoot() )
			kv.depth = depth + 1;

		return kv;
	}

	KeyValues &KeyValues::createKeyValue( const std::string_view &name, const std::string_view &kvValue )
	{
		KeyValues &kv = createKey( name );
		kv.setKeyValueFast( kvValue );

		return kv;
	}

	void KeyValues::removeKey( const std::string &name )
	{
		auto it = keyvalues.find( name );
		if ( it != keyvalues.end() )
			keyvalues.erase( it );
	}

	void KeyValues::removeKey( const std::string &name, size_t index )
	{
		if ( index >= keyvalues.count( name ) )
			return;

		auto range = keyvalues.equal_range( name );
		auto it = range.first;
		std::advance( it, index );

		keyvalues.erase( it );
	}

	KeyValues &KeyValues::get( const std::string &name, size_t index )
	{
		auto range = keyvalues.equal_range( name );

		auto it = range.first;
		std::advance( it, index );

		return *it->second;
	}

	KeyValues &KeyValues::operator[]( const std::string &name )
	{
		if ( auto it = keyvalues.find( name ); it != keyvalues.end() )
			return *it->second;

		return createKey( name );
	}

	size_t KeyValues::getCount( const std::string &name ) const
	{
		return keyvalues.count( name );
	}

	std::string KeyValues::getKeyValue( const std::string &keyName, size_t index, const std::string &defaultVal /*= ""*/ ) const
	{
		const size_t count = keyvalues.count( keyName );

		if ( index >= count )
			return defaultVal;

		auto range = keyvalues.equal_range( keyName );
		auto it = range.first;
		std::advance( it, index );

		return it->second->getValue( defaultVal );
	}

	std::string KeyValues::getKeyValue( const std::string &keyName, const std::string &defaultVal /*= ""*/ ) const
	{
		auto it = keyvalues.find( keyName );
		return it != keyvalues.end() ? it->second->getValue( defaultVal ) : defaultVal;
	}


	KeyValues KeyValues::parseFromFile( const std::string &kvPath, ExpressionEngine expressionEngine /*= ExpressionEngine( true )*/ )
	{
		std::ifstream file( kvPath, std::ios::binary | std::ios::ate );

		if ( !file.is_open() )
			return {};

		const size_t fileSize = file.tellg();
		file.seekg( std::ios::beg );

		std::string buffer( fileSize, '\0' );

		file.read( buffer.data(), buffer.size() );
		file.close();

		return parseFromBuffer( buffer, expressionEngine );
	}

	KeyValues KeyValues::parseFromBuffer( const std::string_view &buffer, ExpressionEngine expressionEngine /*= ExpressionEngine( true )*/ )
	{
		auto getLine = [ &buffer ]( const size_t line ) -> std::string
		{
			size_t startLine = 1;
			size_t index = 0;

			while ( startLine != line && index < buffer.size() )
			{
				if ( buffer[ index ] == '\n' )
					++startLine;

				++index;
			}

			// Uh oh
			if ( index >= buffer.size() )
				return "";

			const size_t start = index;
			while ( ++index < buffer.size() )
			{
				if ( buffer[ index ] == '\n' )
					break;
			}

			const size_t end = index;
			std::string line_str( &buffer[ start ], end - start );

			return line_str;
		};

		auto peekChar = [ &buffer ]( const size_t index ) -> char
		{
			return ( index < buffer.size() ) ? buffer[ index ] : '\0';
		};

		auto skipLineComment = [ &buffer ]( const size_t start ) -> size_t
		{
			for ( size_t i = start; i < buffer.size(); ++i )
			{
				if ( buffer[ i ] == '\n' )
					return i;
			}

			return std::string::npos;
		};

		auto skipMultiLineComment = [ &buffer, &peekChar ]( const size_t start ) -> size_t
		{
			for ( size_t i = start; i < buffer.size(); ++i )
			{
				if ( buffer[ i ] == '/' && peekChar( i - 1 ) == '*' )
					return i;
			}

			return std::string::npos;
		};

		auto skipSection = [ &buffer, &peekChar, &skipLineComment, &skipMultiLineComment ]( const size_t start ) -> size_t
		{
			size_t depth = 0;
			for ( size_t i = start; i < buffer.size(); ++i )
			{
				const char &c = buffer[ i ];
				if ( c == '\"' )
				{
					for ( ++i; i < buffer.size(); ++i )
					{
						if ( buffer[ i ] == '\"' )
							break;
					}

					continue;
				}
				else if ( c == '/' && peekChar( i + 1 ) == '/' )
				{
					i = skipLineComment( i );
					continue;
				}
				else if ( c == '/' && peekChar( i + 1 ) == '*' )
				{
					i = skipMultiLineComment( i );
					continue;
				}
				else if ( c == '{' )
				{
					++depth;
					continue;
				}
				else if ( c == '}' )
				{
					if ( depth == 0 )
						return i;

					--depth;
					continue;
				}
			}

			return std::string::npos;
		};

		auto isWhiteSpace = []( const char &c ) -> bool
		{
			for ( const char &other : cWhiteSpace )
			{
				if ( c == other )
					return true;
			}

			return false;
		};

		auto readUntilNotWhitespace = [ &buffer, &isWhiteSpace ]( size_t index ) -> size_t
		{
			while ( index < buffer.size() )
			{
				if ( !isWhiteSpace( buffer[ index ] ) )
					return index;

				++index;
			}

			return std::string::npos;
		};

		auto readQuote = [ &buffer ]( const size_t start, std::string_view &str ) -> size_t
		{
			size_t len = 0;
			size_t index = start + 1;
			for ( ; index < buffer.size(); ++index )
			{
				const char &c = buffer[ index ];
				if ( c == '"' )
					break;
				else if ( c == '\n' )
					throw ParseException( "Expected '\"' but got EOL instead", ResolveLineColumn( buffer, index ) );

				++len;
			}

			str = std::string_view( &buffer[ start + 1 ], len );
			return ( index + 1 >= buffer.size() ) ? std::string::npos : index + 1;
		};

		// HACKHACK: This is major hack for the parser to determine if 'str' is a control character or not
		constexpr const std::array< char, 8 > specialControl = {
			'\0', '{',
			'\0', '}',
			'\0', '[',
			'\0', ']'
		};

		auto readString = [ &buffer, &readQuote, &isWhiteSpace, &peekChar, &skipLineComment, &skipMultiLineComment, &readUntilNotWhitespace, &specialControl ]( const size_t start, std::string_view &str, auto &readStringRecursive ) -> size_t
		{
			if ( start >= buffer.size() )
			{
				str = "";
				return std::string::npos;
			}

			const char &cStart = buffer[ start ];
			auto findSpecialControl = [ &specialControl, &cStart ]() -> size_t
			{
				for ( size_t i = 1; i < specialControl.size(); i += 2 )
				{
					if ( cStart == specialControl[ i ] )
						return i;
				}

				return 0;
			};

			if ( cStart == '\"' )
				return readQuote( start, str );
			else if ( size_t control = findSpecialControl(); control != 0 )
			{
				str = std::string_view( &specialControl[ control - 1 ], 2 );
				const size_t next = start + 1;

				return ( next >= buffer.size() ) ? std::string::npos : next;
			}
			else if ( cStart == '/' && peekChar( start + 1 ) == '/' )
			{
				size_t next = skipLineComment( start ) + 1;
				next = readUntilNotWhitespace( next );

				return readStringRecursive( next, str, readStringRecursive );
			}
			else if ( cStart == '/' && peekChar( start + 1 ) == '*' )
			{
				size_t next = skipMultiLineComment( start ) + 1;
				next = readUntilNotWhitespace( next );

				return readStringRecursive( next, str, readStringRecursive );
			}

			size_t index = start;
			size_t len = 0;
			for ( ; index < buffer.size(); ++index )
			{
				const char &c = buffer[ index ];
				if ( c == '{' || c == '}' || c == '[' || c == '"' || isWhiteSpace( c ) )
					break;
				else if ( c == '/' && peekChar( index + 1 ) == '/' )
				{
					index = skipLineComment( index );
					continue;
				}
				else if ( c == '/' && peekChar( index + 1 ) == '*' )
				{
					index = skipMultiLineComment( index );
					continue;
				}

				++len;
			}

			str = std::string_view( &buffer[ start ], len );
			return ( index >= buffer.size() ) ? std::string::npos : index;
		};

		KeyValues root;

		auto doParse = [ & ]()
		{
			auto readSection = [ & ]( KeyValues &currentKV, const size_t startSection, auto &readSubSection ) -> size_t
			{
				std::optional< std::string_view > key;
				std::optional< std::string_view > value;
				std::optional< ExpressionEngine::ExpressionResult > expressionResult;

				std::string_view str;
				size_t index = readUntilNotWhitespace( startSection );

				for ( ; index < buffer.size(); index = readUntilNotWhitespace( index ) )
				{
					index = readString( index, str, readString );
					const bool isControlCharacter = ( str.size() == 2 && str[ 0 ] == '\0' ); // Note: This is a total garbage hack

					if ( isControlCharacter )
					{
						const char &control = str[ 1 ];
						switch ( control )
						{
							case '{':
							{
								if ( !key.has_value() )
									throw ParseException( "Unexpected start to subsection", ResolveLineColumn( buffer, index ) );
								else if ( expressionResult.has_value() && !expressionResult->result )
								{
									if ( size_t skip = skipSection( index ); skip == std::string::npos )
										throw ParseException( "Expected '}', got EOF instead", ResolveLineColumn( buffer, index ) );
									else
										index = skip;
								}
								else
								{
									KeyValues &nextKV = currentKV.createKey( key.value() );
									index = readSubSection( nextKV, index, readSubSection );
								}

								key.reset();
								value.reset();
								expressionResult.reset();

								break;
							}
							case '}':
							{
								if ( key.has_value() && !value.has_value() )
									throw ParseException( "Unexpected end to section", ResolveLineColumn( buffer, index ) );
								else if ( key.has_value() && value.has_value() && ( !expressionResult.has_value() || ( expressionResult.has_value() && expressionResult->result ) ) )
									currentKV.createKeyValue( key.value(), value.value() );

								return index;

								break;
							}
							case '[':
							{
								if ( !key.has_value() )
									throw ParseException( "Unexpected start of expression", ResolveLineColumn( buffer, index ) );
								else
								{
									expressionResult = expressionEngine.evaluateExpression( buffer, index - 1 );
									index = expressionResult->end + 1;
								}

								break;
							}
							case ']':
							{
								throw ParseException( "Unexpected expression end ']' token", ResolveLineColumn( buffer, index ) );
								break;
							}
							default:
								break;
						}
					}
					else // Not a control character
					{
						if ( !key.has_value() )
							key = str;
						else if ( !value.has_value() )
							value = str;
						else
						{
							if ( !expressionResult.has_value() || ( expressionResult.has_value() && expressionResult->result ) )
								currentKV.createKeyValue( key.value(), value.value() );

							key = str;
							value.reset();
							expressionResult.reset();
						}
					}
				}

				// Note: We're assuming that if start == 0, we started in 'global' space
				if ( startSection != 0 )
					throw ParseException( "Expected '}', got EOF instead", ResolveLineColumn( buffer, startSection ) );
				else
				{
					if ( key.has_value() && !value.has_value() )
						throw ParseException( "Unexpected end to section", ResolveLineColumn( buffer, buffer.size() - 1 ) );
					else if ( key.has_value() && value.has_value() && ( !expressionResult.has_value() || ( expressionResult.has_value() && expressionResult->result ) ) )
						currentKV.createKeyValue( key.value(), value.value() );
				}

				return index;
			};

			readSection( root, 0, readSection );
		};

		try
		{
			doParse();
		}
		catch ( const ParseException &e )
		{
			if (debugCallback)
			{
				std::stringstream ss;

				ss << "[Line: " << e.getLineNumber() << " Column: " << e.getColumn() << "] ";
				ss << e.what() << std::endl << std::endl;

				std::string line = getLine( e.getLineNumber() );
				size_t tabCount = 0;

				for ( auto it = line.begin(); it != line.end(); ++it )
				{
					if ( *it == '\t' )
						++tabCount;
				}

				line.erase( std::remove( line.begin(), line.end(), '\t' ), line.end() );

				ss << line << std::endl;
				const size_t column = e.getColumn() - tabCount;

				for ( size_t i = 0; i < column; ++i )
					ss << ' ';

				ss << "^\n";
				debugCallback(ss.str());
			}
		}

		return root;
	}

	void KeyValues::saveToFile( const std::string &kvPath )
	{
		KeyValues &root = getRoot();

		if ( root.isEmpty() )
			return;

		std::ofstream file( kvPath );
		std::string buffer;

		saveToBuffer( buffer );

		file << buffer;
	}

	void KeyValues::saveToBuffer( std::string &out )
	{
		KeyValues &root = getRoot();

		if ( root.isEmpty() )
			return;

		std::stringstream ss;

		auto writeTabs = [ &ss ]( size_t tabDepth )
		{
			if ( tabDepth > 0 )
			{
				std::string tabs( tabDepth, '\t' );
				ss << tabs;
			}
		};

		auto writeKey = [ & ]( KeyValues &kv )
		{
			writeTabs( kv.getDepth() );
			ss << '\"' << kv.getKey() << '\"';
		};

		auto writeKV = [ & ]( KeyValues &kv )
		{
			writeKey( kv );
			ss << ' ' << '\"' << kv.getValue() << '\"' << '\n';
		};

		auto writeSectionRecursive = [ & ]( KeyValues &section ) -> void
		{
			auto writeSection = [ & ]( KeyValues &sectionKV, auto &writeFunc ) -> void
			{
				writeKey( sectionKV );

				ss << '\n';
				writeTabs( sectionKV.getDepth() );
				ss << '{' << '\n';

				for ( KeyValues &kv : sectionKV )
				{
					if ( kv.isSection() )
						writeFunc( kv, writeFunc );
					else
						writeKV( kv );
				}

				writeTabs( sectionKV.getDepth() );
				ss << '}' << '\n';

				// Write a new line if it's the end of a 'global' section
				if ( sectionKV.getDepth() == 0 )
					ss << '\n';
			};

			writeSection( section, writeSection );
		};

		for ( KeyValues &kv : root )
		{
			if ( kv.isSection() )
				writeSectionRecursive( kv );
			else
				writeKV( kv );
		}

		out = ss.str();
	}

	void KeyValues::setKeyValue( const std::string &kvValue )
	{
		if ( isSection() )
		{
			for ( auto &kv : keyvalues )
			{
				kv.second->parentKV = parentKV;
				--kv.second->depth;
			}
		}

		value = kvValue;
	}
}