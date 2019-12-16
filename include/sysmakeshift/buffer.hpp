
#ifndef INCLUDED_SYSMAKESHIFT_BUFFER_HPP_
#define INCLUDED_SYSMAKESHIFT_BUFFER_HPP_


#include <cstddef>  // for size_t, ptrdiff_t
#include <memory>   // for unique_ptr<>, allocator<>
#include <utility>  // for move(), in_place (C++17)
#include <iterator> // for random_access_iterator_tag

#include <gsl/gsl-lite.hpp> // for Expects(), span<>, gsl_NODISCARD, gsl_CPP17_OR_GREATER

#include <sysmakeshift/memory.hpp> // for alignment, aligned_allocator<>

#include <sysmakeshift/detail/buffer.hpp>


namespace sysmakeshift
{


    //ᅟ
    // Buffer with aligned elements.
    //ᅟ
    //ᅟ    auto threadData = aligned_buffer<ThreadData, alignment::cache_line>(numThreads);
    //ᅟ    // every `threadData[i]` has cache-line alignment => no false sharing
    //
template <typename T, alignment Alignment, typename A = std::allocator<T>>
class aligned_buffer
{
    static_assert(!std::is_const<T>::value && !std::is_volatile<T>::value, "buffer element type must not have cv qualifiers");
    static_assert(!std::is_reference<T>::value, "buffer element type must not be a reference");

public:
    using allocator_type = aligned_allocator<T, Alignment, A>;

private:
    std::unique_ptr<char[], detail::aligned_buffer_deleter<T, allocator_type>> data_;

public:
    using value_type = T;
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using const_pointer = T const*;
    using reference = T&;
    using const_reference = T const&;

    using iterator = detail::aligned_buffer_iterator<T>;
    using const_iterator = detail::aligned_buffer_iterator<T const>;

    aligned_buffer(void) noexcept
        : data_(detail::acquire_aligned_buffer<T, allocator_type>())
    {
    }
    explicit aligned_buffer(std::size_t _size)
        : data_(detail::acquire_aligned_buffer<T, Alignment, allocator_type>(_size, { }))
    {
    }
    explicit aligned_buffer(std::size_t _size, T const& _value)
        : data_(detail::acquire_aligned_buffer<T, Alignment, allocator_type>(_size, { }, _value))
    {
    }
    explicit aligned_buffer(std::size_t _size, A _allocator)
        : data_(detail::acquire_aligned_buffer<T, Alignment, allocator_type>(_size, std::move(_allocator)))
    {
    }
    explicit aligned_buffer(std::size_t _size, T const& _value, A _allocator)
        : data_(detail::acquire_aligned_buffer<T, Alignment, allocator_type>(_size, std::move(_allocator), _value))
    {
    }
#if gsl_CPP17_OR_GREATER
    template <typename... Ts>
    explicit aligned_buffer(std::size_t _size, std::in_place_t, Ts&&... _args)
        : data_(detail::acquire_aligned_buffer<T, Alignment, allocator_type>(_size, { }, std::forward<Ts>(_args)...))
    {
    }
    template <typename... Ts>
    explicit aligned_buffer(std::size_t _size, A _allocator, std::in_place_t, Ts&&... _args)
        : data_(detail::acquire_aligned_buffer<T, Alignment, allocator_type>(_size, std::move(_allocator), std::forward<Ts>(_args)...))
    {
    }
#endif // gsl_CPP17_OR_GREATER

    gsl_NODISCARD allocator_type get_allocator(void) const noexcept
    {
        return data_.get_deleter().get_allocator();
    }

    gsl_NODISCARD std::size_t size(void) const noexcept
    {
        return data_.get_deleter().size_;
    }
    gsl_NODISCARD reference operator [](std::size_t i)
    {
        Expects(i < data_.get_deleter().size_);

        return reinterpret_cast<reference>(data_.get()[i * data_.get_deleter().bytesPerElement_]);
    }
    gsl_NODISCARD const_reference operator [](std::size_t i) const
    {
        Expects(i < data_.get_deleter().size_);

        return reinterpret_cast<reference>(data_.get()[i * data_.get_deleter().bytesPerElement_]);
    }

    gsl_NODISCARD iterator begin(void) noexcept
    {
        return { data_.get(), 0, data_.get_deleter().bytesPerElement_ };
    }
    gsl_NODISCARD const_iterator begin(void) const noexcept
    {
        return { data_.get(), 0, data_.get_deleter().bytesPerElement_ };
    }
    gsl_NODISCARD iterator end(void) noexcept
    {
        return { data_.get(), data_.get_deleter().size_, data_.get_deleter().bytesPerElement_ };
    }
    gsl_NODISCARD const_iterator end(void) const noexcept
    {
        return { data_.get(), data_.get_deleter().size_, data_.get_deleter().bytesPerElement_ };
    }

    gsl_NODISCARD constexpr bool empty(void) const noexcept
    {
        return data_.get_deleter().size_ == 0;
    }

    gsl_NODISCARD reference front(void)
    {
        Expects(!empty());
        return (*this)[0];
    }
    gsl_NODISCARD const_reference front(void) const
    {
        Expects(!empty());
        return (*this)[0];
    }
    gsl_NODISCARD reference back(void)
    {
        Expects(!empty());
        return (*this)[size() - 1];
    }
    gsl_NODISCARD const_reference back(void) const
    {
        Expects(!empty());
        return (*this)[size() - 1];
    }
};


    //ᅟ
    // Two-dimensional buffer with aligned rows.
    //ᅟ
    //ᅟ    auto threadData = aligned_buffer<float, alignment::cache_line>(rows, cols);
    //ᅟ    // every `threadData[i][0]` has cache-line alignment => no false sharing
    //
template <typename T, alignment Alignment, typename A = std::allocator<T>>
class aligned_row_buffer
{
    static_assert(!std::is_const<T>::value && !std::is_volatile<T>::value, "buffer element type must not have cv qualifiers");
    static_assert(!std::is_reference<T>::value, "buffer element type must not be a reference");

private:
    using allocator_type = aligned_allocator<T, Alignment, A>;

    std::unique_ptr<char[], detail::aligned_row_buffer_deleter<T, allocator_type>> data_;

public:
    using size_type = std::size_t;
    using difference_type = std::ptrdiff_t;
    using reference = gsl::span<T>;
    using const_reference = gsl::span<T const>;

    using iterator = detail::aligned_row_buffer_iterator<T>;
    using const_iterator = detail::aligned_row_buffer_iterator<T const>;

    explicit aligned_row_buffer(void) noexcept
        : data_(detail::acquire_aligned_row_buffer<T, allocator_type>())
    {
    }
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols)
        : data_(detail::acquire_aligned_row_buffer<T, Alignment, allocator_type>(_rows, _cols, { }))
    {
    }
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, T const& _value)
        : data_(detail::acquire_aligned_row_buffer<T, Alignment, allocator_type>(_rows, _cols, { }, _value))
    {
    }
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, A _allocator)
        : data_(detail::acquire_aligned_row_buffer<T, Alignment, allocator_type>(_rows, _cols, std::move(_allocator)))
    {
    }
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, T const& _value, A _allocator)
        : data_(detail::acquire_aligned_row_buffer<T, Alignment, allocator_type>(_rows, _cols, std::move(_allocator), _value))
    {
    }
#if gsl_CPP17_OR_GREATER
    template <typename... Ts>
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, std::in_place_t, Ts&&... _args)
        : data_(detail::acquire_aligned_row_buffer<T, Alignment, allocator_type>(_rows, _cols, { }, std::forward<Ts>(_args)...))
    {
    }
    template <typename... Ts>
    explicit aligned_row_buffer(std::size_t _rows, std::size_t _cols, A _allocator, std::in_place_t, Ts&&... _args)
        : data_(detail::acquire_aligned_row_buffer<T, Alignment, allocator_type>(_rows, _cols, std::move(_allocator), std::forward<Ts>(_args)...))
    {
    }
#endif // gsl_CPP17_OR_GREATER

    gsl_NODISCARD allocator_type get_allocator(void) const noexcept
    {
        return data_.get_deleter().get_allocator();
    }

    gsl_NODISCARD std::size_t rows(void) const noexcept
    {
        return data_.get_deleter().rows_;
    }
    gsl_NODISCARD std::size_t columns(void) const noexcept
    {
        return data_.get_deleter().cols_;
    }

    gsl_NODISCARD std::size_t size(void) const noexcept
    {
        return rows();
    }
    gsl_NODISCARD gsl::span<T> operator [](std::size_t i)
    {
        Expects(i < data_.get_deleter().rows_);

        return { reinterpret_cast<reference>(&data_.get()[i * data_.get_deleter().bytesPerRow_]), data_.get_deleter().cols_ };
    }
    gsl_NODISCARD gsl::span<T const> operator [](std::size_t i) const
    {
        Expects(i < data_.get_deleter().rows_);

        return { reinterpret_cast<reference>(&data_.get()[i * data_.get_deleter().bytesPerRow_]), data_.get_deleter().cols_ };
    }

    gsl_NODISCARD iterator begin(void) noexcept
    {
        return { data_.get(), 0, data_.get_deleter().cols_, data_.get_deleter().bytesPerRow_ };
    }
    gsl_NODISCARD const_iterator begin(void) const noexcept
    {
        return { data_.get(), 0, data_.get_deleter().cols_, data_.get_deleter().bytesPerRow_ };
    }
    gsl_NODISCARD iterator end(void) noexcept
    {
        return { data_.get(), data_.get_deleter().size_, data_.get_deleter().cols_, data_.get_deleter().bytesPerRow_ };
    }
    gsl_NODISCARD const_iterator end(void) const noexcept
    {
        return { data_.get(), data_.get_deleter().size_, data_.get_deleter().cols_, data_.get_deleter().bytesPerRow_ };
    }

    gsl_NODISCARD constexpr bool empty(void) const noexcept
    {
        return data_.get_deleter().rows_ == 0;
    }

    gsl_NODISCARD reference front(void)
    {
        Expects(!empty());
        return (*this)[0];
    }
    gsl_NODISCARD const_reference front(void) const
    {
        Expects(!empty());
        return (*this)[0];
    }
    gsl_NODISCARD reference back(void)
    {
        Expects(!empty());
        return (*this)[size() - 1];
    }
    gsl_NODISCARD const_reference back(void) const
    {
        Expects(!empty());
        return (*this)[size() - 1];
    }
};


} // namespace sysmakeshift


#endif // INCLUDED_SYSMAKESHIFT_BUFFER_HPP_
