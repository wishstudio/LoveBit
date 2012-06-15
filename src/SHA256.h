#include <Qt>

class SHA256
{
public:
    SHA256();

    void update(uint w[]);
    uint h0, h1, h2, h3, h4, h5, h6, h7, h8;
};
