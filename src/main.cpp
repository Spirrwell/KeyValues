#include <iostream>

#include "keyvalues.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

void SerializeTest()
{
	KeyValues root;
	root[ "VertexLitGeneric" ][ "$basetexture" ] = "path/to/VTF";
	root[ "VertexLitGeneric" ][ "$bumpmap" ] = "path/to/other/VTF";

	KeyValues &vlg = root.createKey( "VertexLitGeneric" );
	vlg[ "$basetexture" ] = "other/path";

	root.saveKV( "test_serialize.txt" );
}

void ParseTest()
{
	KeyValues root = KeyValues::parseKVFile( "test_serialize.txt" );
	root.saveKV( "test_serialize_parse.txt" );
}

int main()
{
#ifdef _WIN32
	SetConsoleOutputCP( CP_UTF8 );
#endif

	SerializeTest();
	ParseTest();

	return 0;
}