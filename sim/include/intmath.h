template <class T>
static constexpr std::enable_if_t<std::is_integral_v<T>, int>
floorLog2(T x)
{
    assert(x > 0);

    // A guaranteed unsigned version of x.
    uint64_t ux = (typename std::make_unsigned<T>::type)x;

    int y = 0;
    constexpr auto ts = sizeof(T);

    if (ts >= 8 && (ux & 0xffffffff00000000ULL)) { y += 32; ux >>= 32; }
    if (ts >= 4 && (ux & 0x00000000ffff0000ULL)) { y += 16; ux >>= 16; }
    if (ts >= 2 && (ux & 0x000000000000ff00ULL)) { y +=  8; ux >>=  8; }
    if (ux & 0x00000000000000f0ULL) { y +=  4; ux >>=  4; }
    if (ux & 0x000000000000000cULL) { y +=  2; ux >>=  2; }
    if (ux & 0x0000000000000002ULL) { y +=  1; }

    return y;
}

/**
 * @ingroup api_base_utils
 */
template <class T>
static constexpr int
ceilLog2(const T& n)
{
    assert(n > 0);
    if (n == 1)
        return 0;

    return floorLog2(n - (T)1) + 1;
}