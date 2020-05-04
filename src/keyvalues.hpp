#pragma once

#include <map>
#include <string>
#include <string_view>
#include <fstream>
#include <optional>
#include <filesystem>
#include <type_traits>
#include <cstdint>
#include <array>
#include <exception>
#include <memory>

class KeyValues
{
	using kvString = std::string;
	using kvStringView = std::string_view;
	using kvIFile = std::ifstream;
	using kvOFile = std::ofstream;
	using kvIStream = std::istream;
	using kvOStream = std::ostream;

	constexpr static const kvStringView svMultiLineCommentBegin = "/*";
	constexpr static const kvStringView svMultiLineCommentEnd = "*/";
	constexpr static const kvStringView svSingleLineCommentBegin = "//";
	constexpr static const kvStringView svSingleLineCommentEnd = "\n";
	constexpr static const std::array< char, 3 > cWhiteSpace = { ' ', '\t', '\n' };

	class ParseException : public std::exception
	{
	public:
		struct LineColumn_t
		{
			const size_t line = 0;
			const size_t column = 0;
		};

		ParseException( const std::string &msg, LineColumn_t lineColumn ) : msg( msg ), lineColumn( lineColumn ) {}

		const char *what() const throw() override { return msg.c_str(); }

		size_t getLineNumber() const { return lineColumn.line; }
		size_t getColumn() const { return lineColumn.column; }

	private:
		const std::string msg;
		const LineColumn_t lineColumn;
	};

public:
	KeyValues() = default;
	KeyValues( KeyValues &&other ) noexcept :
		keyvalues( std::move( other.keyvalues ) ),
		key( std::move( other.key ) ),
		value( std::move( other.value ) ),
		parentKV( std::move( other.parentKV ) ),
		depth( std::move( other.depth ) )
	{}

	KeyValues &getRoot();
	KeyValues &getParent() { return *parentKV; }

	bool isRoot() const noexcept { return ( parentKV == nullptr ); }
	bool isEmpty() const noexcept { return keyvalues.empty(); }

	bool hasParent() const noexcept { return !isRoot(); }

	KeyValues &createKey( const kvString &name );
	KeyValues &createKeyValue( const kvString &name, const kvString &kvValue );

	// Beware of dangling references
	void removeKey( const kvString &name ); // Removes first instance of key
	void removeKey( const kvString &name, size_t index ); // Removes key at index if it exists

	KeyValues &get( const kvString &name, size_t index ); // Please ensure getCount( name ) > 0 and index < getCount( name )
	KeyValues &operator[]( const kvString &name );

	KeyValues &operator=( const char *kvValue ) { setKeyValue( kvValue ); return *this; }
	KeyValues &operator=( const kvString &kvValue ) { setKeyValue( kvValue ); return *this; }
	KeyValues &operator=( bool kvValue ) { setKeyValue( toString( kvValue ) ); return *this; }
	KeyValues &operator=( uint8_t kvValue ) { setKeyValue( toString( kvValue ) ); return *this; }
	KeyValues &operator=( uint16_t kvValue ) { setKeyValue( toString( kvValue ) ); return *this; }
	KeyValues &operator=( uint32_t kvValue ) { setKeyValue( toString( kvValue ) ); return *this; }
	KeyValues &operator=( uint64_t kvValue ) { setKeyValue( toString( kvValue ) ); return *this; }
	KeyValues &operator=( int8_t kvValue ) { setKeyValue( toString( kvValue ) ); return *this; }
	KeyValues &operator=( int16_t kvValue ) { setKeyValue( toString( kvValue ) ); return *this; }
	KeyValues &operator=( int32_t kvValue ) { setKeyValue( toString( kvValue ) ); return *this; }
	KeyValues &operator=( int64_t kvValue ) { setKeyValue( toString( kvValue ) ); return *this; }
	KeyValues &operator=( float kvValue ) { setKeyValue( toString( kvValue ) ); return *this; }
	KeyValues &operator=( double kvValue ) { setKeyValue( toString( kvValue ) ); return *this; }

	// Returns number of keys of the specified name we have
	size_t getCount( const kvString &name ) const;

	bool isSection() const { return !value.has_value(); }
	
	kvString getKey() const { return ( key ) ? *key : kvString(); }
	kvString getValue( const kvString &defaultVal = kvString() ) const { return ( value.value_or( defaultVal ) ); }

	kvString getKeyValue( const kvString &keyName, size_t index, const kvString &defaultVal = kvString() ) const;
	kvString getKeyValue( const kvString &keyName, const kvString &defaultVal = kvString() ) const;

	size_t getDepth() const { return depth; }

	static KeyValues parseKV( const std::filesystem::path &kvPath );
	void saveKV( const std::filesystem::path &kvPath );

	void setKeyValue( const kvString &kvValue );

private:

	template< typename T >
	kvString toString( T val )
	{
		return std::to_string( val );
	}

	// Used for creation only because we don't have to reconnect child parents to our parent
	void setKeyValueFast( const kvString &kvValue ) { value = kvValue; }

	std::multimap< kvString, std::unique_ptr< KeyValues > > keyvalues;

	// Pointer to string in parent's multimap
	const kvString *key = nullptr;
	std::optional< kvString > value = std::nullopt;

	KeyValues *parentKV = nullptr;
	size_t depth = 0;
};