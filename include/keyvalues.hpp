#pragma once

#include <map>
#include <unordered_map>
#include <string>
#include <string_view>
#include <fstream>
#include <optional>
#include <type_traits>
#include <cstdint>
#include <array>
#include <exception>
#include <memory>
#include <functional>

namespace KV
{
	void setDebugCallback( std::function< void( const std::string_view &output ) > callback );

	class ExpressionEngine
	{
		friend class KeyValues;
	public:
		struct ExpressionResult
		{
			bool result;
			size_t end;
		};

		ExpressionEngine( bool useAutomaticDefaults = true );

		void setCondition( const std::string &condition, bool value );
		bool getCondition( const std::string &condition ) const;

	protected:
		ExpressionResult evaluateExpression( const std::string_view &expression, const size_t offset = 0 ) const;

	private:
		std::unordered_map< std::string, bool > conditions;
	};

	class KeyValues
	{
		constexpr static const std::array< char, 4 > cWhiteSpace = { ' ', '\t', '\n', '\r' };

	public:

		KeyValues() = default;

		KeyValues( KeyValues &&other ) noexcept :
			key( std::move( other.key ) ),
			value( std::move( other.value ) ),
			parentKV( std::move( other.parentKV ) ),
			depth( std::move( other.depth ) ),
			keyvalues( std::move( other.keyvalues ) )
		{}

		struct kvCompare
		{
			// This should ensure that multimap elements are in order of insertion
			bool operator() ( const std::string&, const std::string& ) const { return false; }
		};

		struct iterator
		{
			using multimap_iterator = std::multimap< std::string, std::unique_ptr< KeyValues >, KeyValues::kvCompare >::iterator;

			iterator( multimap_iterator it ) : it( it ) {}
			iterator operator++() { ++it; return *this; }
			bool operator!=( const iterator &other ) const { return ( it != other.it ); }

			KeyValues &operator*() { return *it->second; }
			const KeyValues &operator*() const { return *it->second; }

		private:
			multimap_iterator it;
		};

		struct const_iterator
		{
			using const_multimap_iterator = std::multimap< std::string, std::unique_ptr< KeyValues >, KeyValues::kvCompare >::const_iterator;

			const_iterator( const_multimap_iterator it ) : it( it ) {}
			const_iterator operator++() { ++it; return *this; }
			bool operator!=( const const_iterator &other ) const { return ( it != other.it ); }

			const KeyValues &operator*() const { return *it->second; }

		private:
			const_multimap_iterator it;
		};

		iterator begin() noexcept { return iterator( keyvalues.begin() ); }
		iterator end() noexcept { return iterator( keyvalues.end() ); }

		const_iterator begin() const noexcept { return const_iterator( keyvalues.cbegin() ); }
		const_iterator end() const noexcept { return const_iterator( keyvalues.cend() ); }

		const_iterator cbegin() const noexcept { return const_iterator( keyvalues.cbegin() ); }
		const_iterator cend() const noexcept { return const_iterator( keyvalues.cend() ); }

		KeyValues &getRoot();
		KeyValues &getParent() { return *parentKV; }

		bool isRoot() const noexcept { return ( parentKV == nullptr ); }
		bool isEmpty() const noexcept { return keyvalues.empty(); }

		bool hasParent() const noexcept { return !isRoot(); }

		KeyValues &createKey( const std::string_view &name );
		KeyValues &createKeyValue( const std::string_view &name, const std::string_view &kvValue );

		// Beware of dangling references
		void removeKey( const std::string &name ); // Removes first instance of key
		void removeKey( const std::string &name, size_t index ); // Removes key at index if it exists

		KeyValues &get( const std::string &name, size_t index ); // Please ensure getCount( name ) > 0 and index < getCount( name )
		KeyValues &operator[]( const std::string &name );

		KeyValues &operator=( const char *kvValue ) { setKeyValue( kvValue ); return *this; }
		KeyValues &operator=( const std::string &kvValue ) { setKeyValue( kvValue ); return *this; }
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
		size_t getCount( const std::string &name ) const;

		bool isSection() const { return !value.has_value(); }
		
		std::string getKey() const { return ( key ) ? *key : std::string(); }
		std::string getValue( const std::string &defaultVal = std::string() ) const { return ( value.value_or( defaultVal ) ); }

		std::string getKeyValue( const std::string &keyName, size_t index, const std::string &defaultVal = std::string() ) const;
		std::string getKeyValue( const std::string &keyName, const std::string &defaultVal = std::string() ) const;

		size_t getDepth() const { return depth; }

		static KeyValues parseFromFile( const std::string &kvPath, ExpressionEngine expressionEngine = ExpressionEngine( true ) );
		static KeyValues parseFromBuffer( const std::string_view &buffer, ExpressionEngine expressionEngine = ExpressionEngine( true ) );

		void saveToFile( const std::string &kvPath );
		void saveToBuffer( std::string &out );

		void setKeyValue( const std::string &kvValue );

	private:

		template< typename T >
		std::string toString( T val )
		{
			return std::to_string( val );
		}

		// Used for creation only because we don't have to reconnect child parents to our parent
		void setKeyValueFast( const std::string_view &kvValue ) { value = kvValue; }

		// Pointer to string in parent's multimap
		const std::string *key = nullptr;
		std::optional< std::string > value = std::nullopt;

		KeyValues *parentKV = nullptr;
		size_t depth = 0;		

		std::multimap< std::string, std::unique_ptr< KeyValues >, kvCompare > keyvalues;
	};

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
}