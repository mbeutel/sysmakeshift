
#ifndef INCLUDED_SYSMAKESHIFT_DETAIL_BUFFER_HPP_
#define INCLUDED_SYSMAKESHIFT_DETAIL_BUFFER_HPP_


#include <limits>
#include <memory>       // for unique_ptr<>
#include <type_traits>  // for common_type<>, is_nothrow_default_constructible<>
#include <system_error> // for errc

#include <gsl/gsl-lite.hpp> // for Expects()

#include <sysmakeshift/memory.hpp> // for alignment


namespace sysmakeshift
{


template <typename T, alignment Alignment, typename A>
class aligned_buffer;

template <typename T, alignment Alignment, typename A>
class aligned_row_buffer;


namespace detail
{


template <typename T>
struct arithmetic_result
{
    T value;
    std::errc ec;
};

template <typename A, typename B>
constexpr arithmetic_result<std::common_type_t<A, B>> multiply_unsigned(A a, B b)
{
    // Borrowed from slowmath.

    using V = std::common_type_t<A, B>;

    if (b > 0 && a > std::numeric_limits<V>::max() / b) return { { }, std::errc::value_too_large };
    return { a * b, { } };
}

    // Computes ⌈x ÷ d⌉ ∙ d for x ∊ ℕ₀, d ∊ ℕ, d ≠ 0.
template <typename X, typename D>
constexpr arithmetic_result<std::common_type_t<X, D>> ceili(X x, D d)
{
    // Borrowed from slowmath.

    using V = std::common_type_t<X, D>;

        // We have the following identities:
        //
        //     x = ⌊x ÷ d⌋ ∙ d + x mod d
        //     ⌈x ÷ d⌉ = ⌊(x + d - 1) ÷ d⌋ = ⌊(x - 1) ÷ d⌋ + 1
        //
        // Assuming x ≠ 0, we can derive the form
        //
        //     ⌈x ÷ d⌉ ∙ d = x + d - (x - 1) mod d - 1 .

    if (x == 0) return { 0, { } };
    V dx = d - (x - 1) % d - 1;
    if (x > std::numeric_limits<V>::max() - dx) return { { }, std::errc::value_too_large };
    return { x + dx, { } };
}


template <typename T, typename A>
struct aligned_buffer_deleter : A // EBO
{
    std::size_t size_; // # elements
    std::size_t bytesPerElement_;
    std::size_t allocSize_; // # bytes

    aligned_buffer_deleter(A _alloc, std::size_t _bytesPerElement, std::size_t _allocSize)
        : A(std::move(_alloc)), size_(0), bytesPerElement_(_bytesPerElement), allocSize_(_allocSize)
    {
    }

    void destroy(char data[])
    {
        for (std::ptrdiff_t i = 0, d = std::ptrdiff_t(bytesPerElement_), e = std::ptrdiff_t(size_) * d; i != e; i += d)
        {
            std::allocator_traits<A>::destroy(*this, reinterpret_cast<T*>(&data[i]));
        }
        size_ = 0;
    }
    void operator ()(char data[])
    {
        using ByteAllocator = typename std::allocator_traits<A>::template rebind_alloc<char>;

        destroy(data);
        std::allocator_traits<ByteAllocator>::deallocate(*this, data, allocSize_);
    }
};

template <typename T, typename A>
std::unique_ptr<char[], aligned_buffer_deleter<T, A>>
init_aligned_buffer(std::unique_ptr<char[], aligned_buffer_deleter<T, A>> data, std::size_t _size, std::size_t _bytesPerElement,
    std::true_type /*isNothrowConstructible*/)
{
    data.get_deleter().size_ = _size;
    for (std::ptrdiff_t i = 0, d = std::ptrdiff_t(_bytesPerElement), e = std::ptrdiff_t(_size * _bytesPerElement); i != e; i += d)
    {
        std::allocator_traits<A>::construct(*this, reinterpret_cast<T*>(&data[i]));
    }
    return data;
}
template <typename T, typename A>
std::unique_ptr<char[], aligned_buffer_deleter<T, A>>
init_aligned_buffer(std::unique_ptr<char[], aligned_buffer_deleter<T, A>> data, std::size_t _size, std::size_t _bytesPerElement,
    std::false_type /*isNothrowConstructible*/)
{
    for (std::ptrdiff_t i = 0, d = std::ptrdiff_t(_bytesPerElement), e = std::ptrdiff_t(_size * _bytesPerElement); i != e; i += d)
    {
        std::allocator_traits<A>::construct(*this, reinterpret_cast<T*>(&data[i]));
        ++data.get_deleter().size_;
    }
    return data;
}

template <typename T, alignment Alignment, typename A>
std::unique_ptr<char[], aligned_buffer_deleter<T, A>>
acquire_aligned_buffer(std::size_t _size, A _allocator)
{
    using ByteAllocator = typename std::allocator_traits<A>::template rebind_alloc<char>;

    auto bytesPerElementR = detail::ceili(sizeof(T), detail::alignment_in_bytes(Alignment));
    auto numBytesR = detail::multiply_unsigned(_size, bytesPerElementR.value);
    if (bytesPerElementR.ec != std::errc{ } || numBytesR.ec != std::errc{ }) throw std::bad_alloc{ };
    auto d = aligned_buffer_deleter(std::move(_allocator), bytesPerElementR.value, numBytesR.value);

    char* rawData = std::allocator_traits<ByteAllocator>::allocate(d, numBytesR.value);
    auto data = std::unique_ptr<char[], aligned_buffer_deleter>(rawData, std::move(d));

    return detail::init_aligned_buffer(data, _size, bytesPerElementR.value, std::is_nothrow_default_constructible<T>{ });
}


template <typename T, typename A>
struct aligned_row_buffer_deleter : A // EBO
{
    std::size_t rows_; // # elements
    std::size_t cols_; // # elements
    std::size_t bytesPerRow_;
    std::size_t elements_; // # elements

    aligned_row_buffer_deleter(A _alloc, std::size_t _rows, std::size_t _cols, std::size_t _bytesPerRow)
        : A(std::move(_alloc)), rows_(_rows), cols_(_cols), bytesPerRow_(_bytesPerRow), elements_(0)
    {
    }

    void destroy(char data[])
    {
        for (std::ptrdiff_t i = 0, d = std::ptrdiff_t(bytesPerRow_), e = std::ptrdiff_t(rows_ * bytesPerRow_), ncols = std::ptrdiff_t(cols_);
                i != e; i += d)
        {
            for (std::ptrdiff_t j = 0; j != ncols; ++j)
            {
                if (elements_-- == 0) return;
                std::allocator_traits<A>::destroy(*this, reinterpret_cast<T*>(&data[i + j]));
            }
        }
    }

    void operator ()(char data[])
    {
        using ByteAllocator = typename std::allocator_traits<A>::template rebind_alloc<char>;

        destroy(data);
        std::allocator_traits<ByteAllocator>::deallocate(*this, data, rows_ * bytesPerRow_);
    }
};

template <typename T, typename A>
std::unique_ptr<char[], aligned_row_buffer_deleter<T, A>>
init_aligned_row_buffer(std::unique_ptr<char[], aligned_row_buffer_deleter<T, A>> data, std::size_t _rows, std::size_t _cols, std::size_t _bytesPerRow,
    std::true_type /*isNothrowConstructible*/)
{
    for (std::ptrdiff_t i = 0, d = std::ptrdiff_t(_bytesPerRow), e = std::ptrdiff_t(_rows * _bytesPerRow), ncols = std::ptrdiff_t(_cols);
            i != e; i += d)
    {
        for (std::ptrdiff_t j = 0; j != ncols; ++j)
        {
            std::allocator_traits<A>::construct(*this, reinterpret_cast<T*>(&data[i + j]));
        }
    }
    data.get_deleter().elements_ = _rows * _cols;
    return data;
}
template <typename T, typename A>
std::unique_ptr<char[], aligned_row_buffer_deleter<T, A>>
init_aligned_row_buffer(std::unique_ptr<char[], aligned_row_buffer_deleter<T, A>> data, std::size_t _rows, std::size_t _cols, std::size_t _bytesPerRow,
    std::false_type /*isNothrowConstructible*/)
{
    for (std::ptrdiff_t i = 0, d = std::ptrdiff_t(_bytesPerRow), e = std::ptrdiff_t(_rows * _bytesPerRow), ncols = std::ptrdiff_t(_cols);
            i != e; i += d)
    {
        for (std::ptrdiff_t j = 0; j != ncols; ++j)
        {
            std::allocator_traits<A>::construct(*this, reinterpret_cast<T*>(&data[i + j]));
            ++data.get_deleter().elements_;
        }
    }
    return data;
}

template <typename T, alignment Alignment, typename A>
std::unique_ptr<char[], aligned_row_buffer_deleter<T, A>>
acquire_aligned_row_buffer(std::size_t _rows, std::size_t _cols, A _allocator)
{
    using ByteAllocator = typename std::allocator_traits<A>::template rebind_alloc<char>;

    auto alignmentInBytes = detail::alignment_in_bytes(Alignment);
    auto rawBytesPerRowR = detail::multiply_unsigned(sizeof(T), _cols);
    auto bytesPerRowR = detail::ceili(rawBytesPerRowR, alignmentInBytes);
    auto numBytesR = detail::multiply_unsigned(_rows, bytesPerRowR.value);
    if (rawBytesPerRowR.ec != std::errc{ } || bytesPerRowR.ec != std::errc{ } || numBytesR.ec != std::errc{ }) throw std::bad_alloc{ };
    auto d = aligned_row_buffer_deleter(std::move(_allocator), _rows, _cols, bytesPerRowR.value);

    char* rawData = std::allocator_traits<ByteAllocator>::allocate(d, numBytesR.value);
    auto data = std::unique_ptr<char[], aligned_row_buffer_deleter>(rawData, std::move(d));

    return detail::init_aligned_row_buffer(data, _rows, _cols, bytesPerRowR.value, std::is_nothrow_default_constructible<T>{ });
}


template <typename T>
class aligned_buffer_iterator
{
    template <typename, alignment, typename> friend class sysmakeshift::aligned_buffer;

private:
    char* data_;
    std::size_t index_;
    std::size_t bytesPerElement_;

    aligned_buffer_iterator(char* _data, std::size_t _index, std::size_t _bytesPerElement)
        : data_(_data), index_(_index), bytesPerElement_(_bytesPerElement)
    {
    }

public:
    constexpr aligned_buffer_iterator(void) noexcept
        : data_(nullptr), index_(0), bytesPerElement_(0)
    {
    }

    aligned_buffer_iterator(aligned_buffer_iterator const&) = default;
    aligned_buffer_iterator& operator =(aligned_buffer_iterator const&) = default;

    template <typename U,
              std::enable_if_t<std::is_const<T>::value && !std::is_const<U>::value && std::is_same<U const, T>::value, int> = 0>
    aligned_buffer_iterator(aligned_buffer_iterator const& rhs) noexcept
        : data_(rhs.data_), index_(rhs.index_), bytesPerElement_(rhs.bytesPerElement_)
    {
    }
    template <typename U,
              std::enable_if_t<std::is_const<T>::value && !std::is_const<U>::value && std::is_same<U const, T>::value, int> = 0>
    aligned_buffer_iterator& operator =(aligned_buffer_iterator const& rhs) noexcept
    {
        data_ = rhs.data_;
        index_ = rhs.index_;
        bytesPerElement_ = rhs.bytesPerElement_;
    }

#ifdef __cpp_lib_concepts
    using iterator_concept  = std::random_access_iterator_tag;
#endif // __cpp_lib_concepts
    using iterator_category = std::random_access_iterator_tag;
    using value_type        = T;
    using difference_type   = typename std::ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;

    friend bool operator ==(aligned_buffer_iterator const& lhs, aligned_buffer_iterator const& rhs)
    {
        Expects(lhs.data_ == rhs.data_);

        return lhs.index_ == rhs.index_;
    }
    friend bool operator !=(aligned_buffer_iterator const& lhs, aligned_buffer_iterator const& rhs)
    {
        return !(lhs == rhs);
    }
    friend bool operator <(aligned_buffer_iterator const& lhs, aligned_buffer_iterator const& rhs)
    {
        Expects(lhs.data_ == rhs.data_);

        return lhs.index_ < rhs.index_;
    }
    friend bool operator >(aligned_buffer_iterator const& lhs, aligned_buffer_iterator const& rhs)
    {
        return rhs < lhs;
    }
    friend bool operator <=(aligned_buffer_iterator const& lhs, aligned_buffer_iterator const& rhs)
    {
        return !(rhs < lhs);
    }
    friend bool operator >=(aligned_buffer_iterator const& lhs, aligned_buffer_iterator const& rhs)
    {
        return !(lhs < rhs);
    }

    gsl_NODISCARD reference operator *(void) const
    {
        return *const_cast<T*>(reinterpret_cast<T const*>(data_ + index_ * bytesPerElement_));
    }
    gsl_NODISCARD pointer operator ->(void) const
    {
        return &**this;
    }
    aligned_buffer_iterator& operator ++(void)
    {
        ++index_;
        return *this;
    }
    aligned_buffer_iterator& operator --(void)
    {
        --index_;
        return *this;
    }
    aligned_buffer_iterator operator ++(int)
    {
        aligned_buffer_iterator result = *this;
        ++index_;
        return result;
    }
    aligned_buffer_iterator operator --(int)
    {
        aligned_buffer_iterator result = *this;
        --index_;
        return result;
    }
    aligned_buffer_iterator& operator +=(difference_type d)
    {
        index_ += d;
        return *this;
    }
    aligned_buffer_iterator operator +(difference_type d) const
    {
        aligned_buffer_iterator result = *this;
        return result += d;
    }
    friend aligned_buffer_iterator operator +(difference_type d, aligned_buffer_iterator const& self)
    {
        aligned_buffer_iterator result = self;
        return result += d;
    }
    aligned_buffer_iterator& operator -=(difference_type d)
    {
        index_ -= d;
        return *this;
    }
    aligned_buffer_iterator operator -(difference_type d) const
    {
        aligned_buffer_iterator result = *this;
        return result -= d;
    }
    difference_type operator -(aligned_buffer_iterator const& rhs) const
    {
        return index_ - rhs.index_;
    }
    reference operator [](difference_type d) const
    {
        return *const_cast<T*>(reinterpret_cast<T const*>(data_ + (index_ + d) * bytesPerElement_));
    }
};


struct input_output_iterator_tag : std::input_iterator_tag, std::output_iterator_tag { };

template <typename T>
class aligned_row_buffer_iterator
{
    template <typename, alignment, typename> friend class sysmakeshift::aligned_row_buffer;

private:
    char* data_;
    std::size_t index_;
    std::size_t cols_;
    std::size_t bytesPerRow_;

    aligned_row_buffer_iterator(char* _data, std::size_t _index, std::size_t _cols, std::size_t _bytesPerRow)
        : data_(_data), index_(_index), cols_(_cols), bytesPerRow_(_bytesPerRow)
    {
    }

public:
    constexpr aligned_row_buffer_iterator(void) noexcept
        : data_(nullptr), index_(0), cols_(0), bytesPerRow_(0)
    {
    }

    aligned_row_buffer_iterator(aligned_row_buffer_iterator const&) = default;
    aligned_row_buffer_iterator& operator =(aligned_row_buffer_iterator const&) = default;

    template <typename U,
              std::enable_if_t<std::is_const<T>::value && !std::is_const<U>::value && std::is_same<U const, T>::value, int> = 0>
    aligned_row_buffer_iterator(aligned_row_buffer_iterator const& rhs) noexcept
        : data_(rhs.data_), index_(rhs.index_), cols_(rhs.cols_), bytesPerRow_(rhs.bytesPerRow_)
    {
    }
    template <typename U,
              std::enable_if_t<std::is_const<T>::value && !std::is_const<U>::value && std::is_same<U const, T>::value, int> = 0>
    aligned_row_buffer_iterator& operator =(aligned_row_buffer_iterator const& rhs) noexcept
    {
        data_ = rhs.data_;
        index_ = rhs.index_;
        cols_ = rhs.cols_;
        bytesPerRow_ = rhs.bytesPerRow_;
    }

#ifdef __cpp_lib_concepts
    using iterator_concept  = std::random_access_iterator_tag; // TODO: is this true?
#endif // __cpp_lib_concepts
    using iterator_category = input_output_iterator_tag;
    using value_type        = gsl::span<T>;
    using difference_type   = typename std::ptrdiff_t;
    using reference         = gsl::span<T>;

    friend bool operator ==(aligned_row_buffer_iterator const& lhs, aligned_row_buffer_iterator const& rhs)
    {
        Expects(lhs.data_ == rhs.data_);

        return lhs.index_ == rhs.index_;
    }
    friend bool operator !=(aligned_row_buffer_iterator const& lhs, aligned_row_buffer_iterator const& rhs)
    {
        return !(lhs == rhs);
    }
    friend bool operator <(aligned_row_buffer_iterator const& lhs, aligned_row_buffer_iterator const& rhs)
    {
        Expects(lhs.data_ == rhs.data_);

        return lhs.index_ < rhs.index_;
    }
    friend bool operator >(aligned_row_buffer_iterator const& lhs, aligned_row_buffer_iterator const& rhs)
    {
        return rhs < lhs;
    }
    friend bool operator <=(aligned_row_buffer_iterator const& lhs, aligned_row_buffer_iterator const& rhs)
    {
        return !(rhs < lhs);
    }
    friend bool operator >=(aligned_row_buffer_iterator const& lhs, aligned_row_buffer_iterator const& rhs)
    {
        return !(lhs < rhs);
    }

    gsl_NODISCARD reference operator *(void) const
    {
        return { const_cast<T*>(reinterpret_cast<T const*>(data_ + index_ * bytesPerRow_)), cols_ };
    }
    aligned_row_buffer_iterator& operator ++(void)
    {
        ++index_;
        return *this;
    }
    aligned_row_buffer_iterator& operator --(void)
    {
        --index_;
        return *this;
    }
    aligned_row_buffer_iterator operator ++(int)
    {
        aligned_row_buffer_iterator result = *this;
        ++index_;
        return result;
    }
    aligned_row_buffer_iterator operator --(int)
    {
        aligned_row_buffer_iterator result = *this;
        --index_;
        return result;
    }
    aligned_row_buffer_iterator& operator +=(difference_type d)
    {
        index_ += d;
        return *this;
    }
    aligned_row_buffer_iterator operator +(difference_type d) const
    {
        aligned_row_buffer_iterator result = *this;
        return result += d;
    }
    friend aligned_row_buffer_iterator operator +(difference_type d, aligned_row_buffer_iterator const& self)
    {
        aligned_row_buffer_iterator result = self;
        return result += d;
    }
    aligned_row_buffer_iterator& operator -=(difference_type d)
    {
        index_ -= d;
        return *this;
    }
    aligned_row_buffer_iterator operator -(difference_type d) const
    {
        aligned_row_buffer_iterator result = *this;
        return result -= d;
    }
    difference_type operator -(aligned_row_buffer_iterator const& rhs) const
    {
        return index_ - rhs.index_;
    }
    reference operator [](difference_type d) const
    {
        return { const_cast<T*>(reinterpret_cast<T const*>(data_ + (index_ + d) * bytesPerRow_)), cols_ };
    }
};


} // namespace detail

} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_DETAIL_BUFFER_HPP_
