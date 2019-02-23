#ifndef ZAIMONI_STL_GDI_BOX_HPP
#define ZAIMONI_STL_GDI_BOX_HPP 1

#include <functional>

namespace zaimoni {
namespace gdi {

template<class T>	// T assumed to be "like a vector space"
class box
{
private:
	T _top_left;		// nonstrict bounds
	T _bottom_right;
public:
	box() = default;
	box(const T& tl, const T& br) : _top_left(tl), _bottom_right(br) {};
	box(const box& src) = default;
	~box() = default;
	box& operator=(const box& src) = default;

	const T& tl_c() const { return _top_left; }
	const T& br_c() const { return _bottom_right; }

	bool contains(const T& src) const {
		static std::less_equal<T::coord_type> lte;
		return pointwise_test(_top_left, src, lte) && pointwise_test(src, _bottom_right, lte);
	}

	box& operator+=(const T& rhs) {
		_top_left += rhs;
		_bottom_right += rhs;
		return *this;
	}

	box& operator-=(const T& rhs) {
		_top_left -= rhs;
		_bottom_right -= rhs;
		return *this;
	}

	box& operator*=(const typename T::coord_type& rhs) {
		_top_left *= rhs;
		_bottom_right *= rhs;
		return *this;
	}

	box& operator/=(const typename T::coord_type& rhs) {
		_top_left /= rhs;
		_bottom_right /= rhs;
		return *this;
	}
};

}	// namespace gdi
}	// namespace zaimoni

#endif