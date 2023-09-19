/* Copyright (C) 1994 DJ Delorie, see COPYING.DJ for details */
#include <precomp.h>

/*
 * @implemented
 */
int
CDECL
_memicmp(const void *s1, const void *s2, size_t n)
{
#if (DLL_EXPORT_VERSION >= 0x600)
    if (!s1 || !s2)
    {
        if (n)
            MSVCRT_INVALID_PMT(NULL, EINVAL);
        return n ? _NLSCMPERROR : 0;
    }
#endif

  if (n != 0)
  {
    const unsigned char *p1 = s1, *p2 = s2;

    do {
      if (toupper(*p1) != toupper(*p2))
	return (*p1 - *p2);
      p1++;
      p2++;
    } while (--n != 0);
  }
  return 0;
}
