/**
* @author: Matthaeus G. "Anteru" Chajdas
* Imported from niven
*/

#ifndef KYLA_CORE_INTERNAL_ARRAYREF_H_E5E291B7947188C18F5ECD8B401909B2B39D43D7
#define KYLA_CORE_INTERNAL_ARRAYREF_H_E5E291B7947188C18F5ECD8B401909B2B39D43D7

#include <cstddef>
#include <vector>

#include "ArrayAdapter.h"
#include "Types.h"

namespace kyla {
/**
@ingroup Core
@brief Wrapper class for read-only memory buffers.

The ArrayRef class provides a reference to an continuous array of data. It
stores both the size and the start pointer into the array. The ArrayRef provides
a read-only view and abstracts from the underlying storage; allowing functions
to consume \c std::vector instances just as easy as static arrays.

ArrayRef does not copy the data, but relies on it being available throughout the
lifetime of the ArrayRef pointing to it.

For mutable data, see MutableArrayRef.

*/
template <typename T = void>
class ArrayRef
{
public:
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef std::ptrdiff_t size_type;
	typedef std::ptrdiff_t count_type;

	template <typename U>
	ArrayRef (const U& t,
		typename ArrayAdapter<U>::Type* = nullptr)
		: data_ (ArrayAdapter<U>::GetDataPointer (t))
		, count_ (ArrayAdapter<U>::GetCount (t))
	{
	}

	ArrayRef (const T* begin, const T* end)
		: data_ (begin)
		, count_ (end-begin)
	{
	}

	ArrayRef (const T* data, const count_type count)
		: data_ (data)
		, count_ (count)
	{
	}

	ArrayRef (const count_type count, const T* data)
		: data_ (data)
		, count_ (count)
	{
	}

	ArrayRef (const std::vector<T>& vec)
		: data_ (vec.data ())
		, count_ (vec.size ())
	{
	}

	template <int N>
	ArrayRef (const T (&array)[N])
		: data_ (array)
		, count_ (N)
	{
	}

	explicit ArrayRef (const T& element)
		: data_ (&element)
		, count_ (1)
	{
	}

	ArrayRef ()
		: data_ (nullptr)
		, count_ (0)
	{
	}

	template <typename IndexType>
	const T& operator [] (const IndexType index) const
	{
		return data_ [index];
	}

	/**
	* Get the number of elements referenced.
	*
	* @note This is not the size in bytes. The memory size can be queried
	*	using GetSize().
	*/
	count_type GetCount () const
	{
		return count_;
	}

	/**
	* Get the size of the referenced data in bytes.
	*
	* @note This is not the element count. The number of elements can be
	*	obtained using GetCount().
	*/
	size_type GetSize () const
	{
		return sizeof (T) * count_;
	}

	const T* GetData () const
	{
		return data_;
	}

	const_iterator begin () const
	{
		return data_;
	}

	const_iterator cbegin () const
	{
		return data_;
	}

	const_iterator end () const
	{
		return data_ + count_;
	}

	const_iterator cend () const
	{
		return data_ + count_;
	}

	bool empty () const
	{
		return count_ == 0;
	}

	bool IsEmpty () const
	{
		return empty ();
	}

	/**
	Slice the array.

	Returns a sub-range into this array, starting at \c first and
	containing \c count elements.
	*/
	ArrayRef Slice (size_type first, size_type count) const
	{
		return ArrayRef (data_ + first, data_ + first + count);
	}

	/**
	Slice the array.

	Returns a sub-range from this array, starting at \c first and going to
	the end.
	*/
	ArrayRef Slice (size_type first) const
	{
		return ArrayRef (data_ + first, data_ + count_);
	}

private:
	const T*	data_;
	count_type	count_;
};

template <>
class ArrayRef<void>
{
public:
	typedef ptrdiff_t size_type;

	template <typename T>
	ArrayRef (const ArrayRef<T>& ref)
		: data_ (ref.GetData ())
		, size_ (ref.GetSize ())
	{
	}

	template <typename U>
	ArrayRef (const U& t,
		typename ArrayAdapter<U>::Type* = nullptr)
		: data_ (ArrayAdapter<U>::GetDataPointer (t))
		, size_ (ArrayAdapter<U>::GetSize (t))
	{
	}

	ArrayRef (const void* data, const size_type size)
		: data_ (data)
		, size_ (size)
	{
	}

	ArrayRef (const size_type size, const void* data)
		: data_ (data)
		, size_ (size)
	{
	}

	template <typename T>
	ArrayRef (const std::vector<T>& vec)
		: data_ (vec.data ())
		, size_ (vec.size () * sizeof (T))
	{
	}

	template <typename T, int N>
	ArrayRef (const T (&array)[N])
		: data_ (array)
		, size_ (N * sizeof (T))
	{
	}

	ArrayRef ()
		: data_ (nullptr)
		, size_ (0)
	{
	}

	size_type GetSize () const
	{
		return size_;
	}

	const void* GetData () const
	{
		return data_;
	}

	bool empty () const
	{
		return size_ == 0;
	}

	bool IsEmpty () const
	{
		return empty ();
	}

	ArrayRef<byte> ToByteRef () const
	{
		return ArrayRef<byte> (
			static_cast<const byte*> (data_),
			static_cast<const byte*> (data_) + size_);
	}

private:
	const void*	data_;
	size_type	size_;
};

/**
@ingroup Core
@brief Wrapper class for modifiable memory buffers.

The MutableArrayRef is an extension to an ArrayRef, providing read-write access
to the data.

MutableArrayRefs are designed to be passed by const reference or by value.
Passing by const reference still allows read-write access to the data and
enables temporary instances to be passed to functions.

For read-only data, see ArrayRef.

*/
template <typename T = void>
class MutableArrayRef : public ArrayRef<T>
{
public:
	typedef T* iterator;
	typedef typename ArrayRef<T>::size_type size_type;

	MutableArrayRef (T* begin, T* end)
	: ArrayRef<T> (begin, end)
	{
	}

	template <typename U>
	MutableArrayRef (U& t)
	: ArrayRef<T> (t)
	{
	}

	MutableArrayRef (T* data, const typename ArrayRef<T>::count_type count)
	: ArrayRef<T> (data, count)
	{
	}

	MutableArrayRef (const typename ArrayRef<T>::count_type count, T* data)
	: ArrayRef<T> (count, data)
	{
	}

	MutableArrayRef (std::vector<T>& vec)
	: ArrayRef<T> (vec)
	{
	}

	template <int N>
	MutableArrayRef (T (&array)[N])
	: ArrayRef<T> (array)
	{
	}

	MutableArrayRef ()
	: ArrayRef<T> ()
	{
	}

	template <typename IndexType>
	T& operator [] (const IndexType index) const
	{
		return GetData () [index];
	}

	T* GetData () const
	{
		return const_cast<T*> (ArrayRef<T>::GetData ());
	}

	iterator begin () const
	{
		return GetData ();
	}

	iterator end () const
	{
		return GetData () + this->GetCount ();
	}

	/**
	@copydoc ArrayRef::Slice(size_type first, size_type count) const
	*/
	MutableArrayRef Slice (size_type first, size_type count) const
	{
		return MutableArrayRef (GetData() + first, GetData () + first + count);
	}

	/**
	@copydoc ArrayRef::Slice(size_type first) const
	*/
	MutableArrayRef Slice (size_type first) const
	{
		return MutableArrayRef (GetData () + first, GetData () + this->GetCount ());
	}
};

template <>
class MutableArrayRef<void> : public ArrayRef<void>
{
public:
	typedef ptrdiff_t size_type;

	template <typename T>
	MutableArrayRef (const MutableArrayRef<T>& ref)
	: ArrayRef<void> (ref)
	{
	}

	template <typename U>
	MutableArrayRef (U& t)
	: ArrayRef<void> (t)
	{
	}

	MutableArrayRef (void* data, const size_type size)
	: ArrayRef<void> (data, size)
	{
	}

	MutableArrayRef (const size_type size, void* data)
	: ArrayRef<void> (data, size)
	{
	}

	template <typename T>
	MutableArrayRef (std::vector<T>& vec)
	: ArrayRef<void> (vec)
	{
	}

	template <typename T, int N>
	MutableArrayRef (T (&array)[N])
	: ArrayRef<void> (array)
	{
	}

	MutableArrayRef ()
	: ArrayRef<void> ()
	{
	}

	void* GetData () const
	{
		return const_cast<void*> (ArrayRef<void>::GetData ());
	}
};
} // namespace kyla
#endif

