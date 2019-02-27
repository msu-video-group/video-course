You will need Visual Studio 2015 Update 3.
Put your name into the constants in motion_estimator.hpp.
Put your algorithm into motion_estimator.cpp.

Look for ME_performance.log and ME_PSNR.log in your current folder or VirtualDub folder
for performance results and PSNR results (if enabled).

Script configuration parameters:
VirtualDub.video.filters.instance[0].Config(2, 0, 0, 0, 100, 0);

First argument: output type
 - 0: Show source
 - 1: Show residual before motion compensation
 - 2: Show residual after motion compensation
 - 3: Show compensated frame

Second argument: show motion vectors
 - 0: Don't show
 - 1: Show

Third argument: draw nothing
 - 0: Disabled (do draw output)
 - 1: Enabled (draw nothing)

Fourth argument: PSNR measurement
 - 0: Disabled
 - 1: Enabled

Fifth argument: algorithm quality
 - valid values: integers from 0 to 100, inclusive

Sixth argument: use half-pixel precision
 - 0: Do not use half-pixel prevision
 - 1: Use half-pixel precision
