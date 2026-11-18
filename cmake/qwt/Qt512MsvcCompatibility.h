#pragma once
#include <QtCore/qglobal.h>

// Qt 5.12 references checked iterators removed by recent MSVC standard libraries.
// Older toolsets keep their existing implementation.
#if defined(_MSC_VER) && _MSC_VER >= 1940
#undef QT_MAKE_CHECKED_ARRAY_ITERATOR
#undef QT_MAKE_UNCHECKED_ARRAY_ITERATOR
#define QT_MAKE_CHECKED_ARRAY_ITERATOR(pointer, size) (pointer)
#define QT_MAKE_UNCHECKED_ARRAY_ITERATOR(pointer) (pointer)
#endif
