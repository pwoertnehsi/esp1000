#ifndef ESP1000_DIAGNOSTICS_H
#define ESP1000_DIAGNOSTICS_H
/* Existing developer builds keep diagnostics; release builds explicitly opt out. */
#ifndef ESP1000_DIAGNOSTICS
#define ESP1000_DIAGNOSTICS 1
#endif
#if ESP1000_DIAGNOSTICS != 0 && ESP1000_DIAGNOSTICS != 1
#error ESP1000_DIAGNOSTICS must be 0 or 1
#endif
#endif
