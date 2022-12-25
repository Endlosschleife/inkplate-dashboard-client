
#include "Inkplate.h"

class Pictureframe
{
private:
    Inkplate &display;

public:
    Pictureframe(Inkplate &d): display(d) { }
    void show();
};