#include <iostream>

#include "keyvalues.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

void DebugCallback( const std::string_view &output )
{
	std::cout << output;
}

void SerializeTest()
{
	KV::KeyValues root;
	root[ "VertexLitGeneric" ][ "$basetexture" ] = "path/to/VTF";
	root[ "VertexLitGeneric" ][ "$bumpmap" ] = "path/to/other/VTF";

	KV::KeyValues &vlg = root.createKey( "VertexLitGeneric" );
	vlg[ "$basetexture" ] = "other/path";

	root.saveToFile( "test_serialize.txt" );
}

void ParseFileTest()
{
	KV::KeyValues root = KV::KeyValues::parseFromFile( "test_serialize.txt" );
	root.saveToFile( "test_serialize_parse.txt" );
}

void ParseStringTest()
{
	const std::string test =
	R"(VertexLitGeneric
		{
			$basetexture "path/to/vtf"
		}
	)";

	KV::KeyValues root = KV::KeyValues::parseFromBuffer( test );

	std::string buffer;
	root.saveToBuffer( buffer );

	std::cout << "Parse from string test:" << std::endl;
	std::cout << buffer << std::endl;
}

void ParseErrorTest()
{
	const std::string test =
	R"(Hello
		{
			"quote" error"
		}
	)";

	std::cout << "Parse error check test:" << std::endl;
	KV::KeyValues::parseFromBuffer( test );
}

int main()
{
#ifdef _WIN32
	SetConsoleOutputCP( CP_UTF8 );
#endif

	KV::setDebugCallback( &DebugCallback );

	SerializeTest();
	ParseFileTest();
	ParseStringTest();
	ParseErrorTest();

	return 0;
}