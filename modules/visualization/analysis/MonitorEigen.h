#ifndef AEROSIM_MONITOR_EIGEN_H
#define AEROSIM_MONITOR_EIGEN_H

// One place that knows how Eigen is laid out on this machine.
//
// VTK 8.2 ships its own Eigen 3.3.4 under ThirdParty/eigen/vtkeigen, rearranged
// so the public headers sit directly in vtkeigen/ (Eigen, Dense and Core are
// files there, not directories). With that root on the include path the header
// is <eigen/Dense>. A stock Eigen install instead exposes <Eigen/Dense>.
//
// Both spellings are tried so the module builds against either. On Windows the
// difference is invisible (the file system is case insensitive), but on Linux it
// decides whether the include resolves at all.
#if defined(__has_include)
#  if __has_include(<eigen/Dense>)
#    include <eigen/Dense>
#  elif __has_include(<Eigen/Dense>)
#    include <Eigen/Dense>
#  else
#    error "Eigen was not found. Point AEROSIM_EIGEN_ROOT at an Eigen 3 install, or at VTK's ThirdParty/eigen/vtkeigen."
#  endif
#else
#  include <Eigen/Dense>
#endif

#endif // AEROSIM_MONITOR_EIGEN_H
