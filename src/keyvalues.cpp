#include "keyvalues.hpp"
#include <iostream>

KeyValues &KeyValues::getRoot()
{
	if ( isRoot() )
		return *this;
	
	auto root = parentKV;
	while ( root->parentKV != nullptr )
		root = root->parentKV;
	
	return *root;
}

KeyValues &KeyValues::createKey( const kvString &name )
{
	auto it = keyvalues.insert( std::make_pair( kvString( name ), KeyValues() ) );
	KeyValues &kv = it->second;
	kv.key = &it->first;
	kv.parentKV = this;

	if ( !isRoot() )
		kv.depth = depth + 1;

	return kv;
}

KeyValues &KeyValues::createKeyValue( const kvString &name, const kvString &kvValue )
{
	KeyValues &kv = createKey( name );
	kv.setKeyValueFast( kvValue );

	return kv;
}

void KeyValues::removeKey( const kvString &name )
{
	auto it = keyvalues.find( name );
	if ( it != keyvalues.end() )
		keyvalues.erase( it );
}

void KeyValues::removeKey( const kvString &name, size_t index )
{
	auto it = keyvalues.find( name );
	if ( it != keyvalues.end() )
	{
		const size_t bucket = keyvalues.bucket( name );
		const size_t bucketSize = keyvalues.bucket_size( bucket );

		if ( index < bucketSize )
		{
			auto bucketIt = keyvalues.begin( bucket );
			std::advance( bucketIt, index );

			keyvalues.erase( bucketIt );
		}
	}
}

KeyValues &KeyValues::get( const kvString &name, size_t index )
{
	size_t bucket = keyvalues.bucket( name );
	auto it = keyvalues.begin( bucket );

	std::advance( it, index );
	return it->second;
}

KeyValues &KeyValues::operator[]( const kvString &name )
{
	if ( auto it = keyvalues.find( name ); it != keyvalues.end() )
		return it->second;
	
	return createKey( name );
}

size_t KeyValues::getCount( const kvString &name ) const
{
	if (  keyvalues.find( name ) != keyvalues.end() && keyvalues.bucket_count() > 0 )
		return keyvalues.bucket_size( keyvalues.bucket( name ) );
	
	return 0;
}

KeyValues::kvString KeyValues::getKeyValue( const kvString &keyName, size_t index, const kvString &defaultVal /*= ""*/ ) const
{
	auto it = keyvalues.find( keyName );
	if ( it != keyvalues.cend() )
	{
		if ( keyvalues.bucket_count() > 0 )
		{
			const size_t bucket = keyvalues.bucket( keyName );
			const size_t bucketSize = keyvalues.bucket_size( bucket );

			if ( index < bucketSize )
			{
				auto bucketIt = keyvalues.cbegin( bucket );
				std::advance( bucketIt, index );

				return bucketIt->second.getValue( defaultVal );
			}
		}
	}

	return defaultVal;
}

KeyValues::kvString KeyValues::getKeyValue( const kvString &keyName, const kvString &defaultVal /*= ""*/ ) const
{
	auto it = keyvalues.find( keyName );
	return it != keyvalues.end() ? it->second.getValue( defaultVal ) : defaultVal;
}


KeyValues KeyValues::parseKV( const std::filesystem::path &kvPath )
{
	const auto fileSize = std::filesystem::file_size( kvPath );

	if ( fileSize == 0 )
		return {};

	kvIFile file( kvPath, std::ios::binary );

	if ( !file.is_open() )
		return {};
	
	kvString buffer( fileSize, '\0' );

	file.read( buffer.data(), buffer.size() );
	file.close();

	auto getLine = [ &buffer ]( const size_t line ) -> kvString
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
		kvString line_str( &buffer[ start ], end - start );

		return line_str;
	};

	auto resolveLineColumn = [ &buffer ]( size_t index ) -> ParseException::LineColumn_t
	{
		constexpr auto UTF8_MB_CONTINUE = 2;

		if ( index == kvString::npos || index >= buffer.size() )
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
			else if ( ( c >> 6 ) != UTF8_MB_CONTINUE )
				++column;
			
		}

		return ParseException::LineColumn_t{ line, column };
	};

	// TODO: We shouldn't remove comments because this screws up our line and column numbers for errors
	auto removeComments = [ &buffer ]()
	{
		auto stripComment = [ &buffer ]( kvStringView commentBegin, kvStringView commentEnd )
		{
			for ( size_t index = buffer.find( commentBegin ); index != kvString::npos; index = buffer.find( commentBegin, index ) )
			{
				bool inQuote = false;
				for ( size_t qIndex = 0; qIndex < index; ++qIndex )
				{
					const char &c = buffer[ qIndex ];
					if ( c == '\"' )
						inQuote = !inQuote;
					else if ( c == '\n' )
						inQuote = false;
				}

				if ( !inQuote )
				{
					const size_t end = buffer.find( commentEnd, index + commentBegin.size() );

					if ( end == kvString::npos )
						buffer.erase( buffer.begin() + index, buffer.end() );
					else
						buffer.erase( index, end - index + commentEnd.size() );
					
					index = 0;
				}
				else if ( ++index >= buffer.size() )
					index = kvString::npos;
			}
		};

		stripComment( svSingleLineCommentBegin, svSingleLineCommentEnd );
		stripComment( svMultiLineCommentBegin, svMultiLineCommentEnd );
	};

	auto removeCharacter = [ &buffer ]( const char &c )
	{
		buffer.erase( std::remove( buffer.begin(), buffer.end(), c ), buffer.end() );
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

		return kvString::npos;
	};

	auto readUntilWhiteSpaceOrBrace = [ &buffer, &isWhiteSpace ]( size_t index ) -> size_t
	{
		while ( index < buffer.size() )
		{
			const char &c = buffer[ index ];
			if ( isWhiteSpace( c ) || c == '{' || c == '}' )
				return index;
		}

		return kvString::npos;
	};

	auto readUntilEndQuote = [ &buffer, &resolveLineColumn ]( size_t index ) -> size_t
	{
		const size_t start = index;
		while ( index < buffer.size() )
		{
			const char &c = buffer[ index ];
			
			if ( c == '\"' )
				return index;
			else if ( c == '\n' )
				throw ParseException( "Expected '\"' but got EOL instead", resolveLineColumn( index ) );
			
			++index;
		}

		throw ParseException( "Expected '\"' but got EOF instead", resolveLineColumn( start ) );
	};

	// Reads next string until we hit control character or whitespace, if string starts with quote, reads until end quote
	auto readNextString = [ &buffer, &readUntilEndQuote, &isWhiteSpace, &resolveLineColumn ]( size_t index ) -> size_t
	{
		const size_t start = index;
		const char &cStart = buffer[ start ];

		if ( cStart == '\"' )
			return readUntilEndQuote( start + 1 );
		else if ( cStart == '{' || cStart == '}' )
			return start;

		while ( ++index < buffer.size() )
		{
			const char &c = buffer[ index ];

			if ( c == '{' || c == '}' || c == '\"' || isWhiteSpace( c ) )
				return index;
		}

		throw ParseException( "Expected end of string, got EOF instead", resolveLineColumn( start ) );
	};

	auto constructKeyName = [ &buffer ]( size_t start, size_t end )
	{
		return kvString( buffer, start, end - start );
	};

	removeComments();
	removeCharacter( '\r' );

	KeyValues root;

	auto doParse = [ & ]()
	{
		auto readSection = [ & ]( KeyValues &currentKV, const size_t startSection, auto &readSubSection ) -> size_t
		{
			size_t index = startSection;
			while ( index != kvString::npos && index < buffer.size() )
			{
				if ( index = readUntilNotWhitespace( index ); index == kvString::npos )
					break;

				const size_t end = readNextString( index );
				if ( end == index )
				{
					const char &c = buffer[ end ];

					if ( c == '{' )
						throw ParseException( "Unexpected start to subsection", resolveLineColumn( end ) );
					else if ( c == '}' )
						return end + 1;
				}
				else
				{
					const size_t firstStart = ( buffer[ index ] == '\"' ) ? index + 1 : index;
					const size_t firstEnd = end;

					index = ( buffer[ end ] == '\"' ) ? readUntilNotWhitespace( end + 1 ) : readUntilNotWhitespace( end );

					if ( index == kvString::npos )
						throw ParseException( "Expected value or new subsection", resolveLineColumn( end ) );
					
					const size_t nextEnd = readNextString( index );
					if ( nextEnd == index )
					{
						const char &c = buffer[ nextEnd ];

						if ( c == '}' )
							throw ParseException( "Unexpected end to section", resolveLineColumn( nextEnd ) );
						else if ( c == '{' )
						{
							KeyValues &nextKV = currentKV.createKey( constructKeyName( firstStart, firstEnd ) );
							index = readSubSection( nextKV, nextEnd + 1, readSubSection );
						}
					}
					else
					{
						const size_t nextStart = ( buffer[ index ] == '\"' ) ? index + 1 : index;
						currentKV.createKeyValue( constructKeyName( firstStart, firstEnd ), constructKeyName( nextStart, nextEnd ) );
						index = ( buffer[ nextEnd ] == '\"' ) ? nextEnd + 1 : nextEnd;
					}
				}
			}

			// Note: We're assuming that if start == 0, we started in 'global' space
			if ( startSection != 0 && index >= buffer.size() )
				throw ParseException( "Expected '}', got EOF instead", resolveLineColumn( startSection ) );
			
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
		std::cout << "[Line: " << e.getLineNumber() << " Column: " << e.getColumn() << "] ";
		std::cout << e.what() << std::endl << std::endl;

		kvString line = getLine( e.getLineNumber() );
		size_t tabCount = 0;

		for ( auto it = line.begin(); it != line.end(); ++it )
		{
			if ( *it == '\t' )
				++tabCount;
		}

		line.erase( std::remove( line.begin(), line.end(), '\t' ), line.end() );

		std::cout << line << std::endl;
		const size_t column = e.getColumn() - tabCount;

		for ( size_t i = 0; i < column; ++i )
			std::cout << ' ';
		
		std::cout << "^\n";
	}

	return root;
}

void KeyValues::saveKV( const std::filesystem::path &kvPath )
{
	KeyValues &root = getRoot();

	if ( root.isEmpty() )
		return;
	
	kvOFile file( kvPath );

	auto writeTabs = [ &file ]( size_t tabDepth )
	{
		if ( tabDepth > 0 )
		{
			kvString tabs( tabDepth, '\t' );
			file << tabs;
		}
	};

	auto writeKey = [ & ]( KeyValues &kv )
	{
		writeTabs( kv.getDepth() );
		file << '\"' << kv.getKey() << '\"';
	};

	auto writeKV = [ & ]( KeyValues &kv )
	{
		writeKey( kv );
		file << ' ' << '\"' << kv.getValue() << '\"' << '\n';
	};

	auto writeSectionRecursive = [ & ]( KeyValues &section ) -> void
	{
		auto writeSection = [ & ]( KeyValues &sectionKV, auto &writeFunc ) -> void
		{
			writeKey( sectionKV );

			file << '\n';
			writeTabs( sectionKV.getDepth() );
			file << '{' << '\n';

			for ( auto &kv : sectionKV.keyvalues )
			{
				if ( kv.second.isSection() )
					writeFunc( kv.second, writeFunc );
				else
					writeKV( kv.second );
			}

			writeTabs( sectionKV.getDepth() );
			file << '}' << '\n';

			// Write a new line if it's the end of a 'global' section
			if ( sectionKV.getDepth() == 0 )
				file << '\n';
		};

		writeSection( section, writeSection );
	};

	for ( auto &kv : root.keyvalues )
	{
		if ( kv.second.isSection() )
			writeSectionRecursive( kv.second );
		else
			writeKV( kv.second );
	}
}

void KeyValues::setKeyValue( const kvString &kvValue )
{
	if ( isSection() )
	{
		for ( auto &kv : keyvalues )
		{
			kv.second.parentKV = parentKV;
			--kv.second.depth;
		}
	}

	value = kvValue;
}