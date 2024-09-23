# EyeBot Feature Test Program

## Usage Instructions

The initial home screen shows a vertical list of menu options that the user can
select individually, taking them into unique demos. The labels for these demos are listed
below as headings, along with explanations.

### `CAMERA`

This demo tests the `CAMGet()` and `CAMGetGray()` library functions, along 
with the `LCDImage()` function to display the camera feed to the screen.
A frames-per-second (FPS) counter is also printed to the screen as an approximate
measure of the rate at which images can be retrieved from the ESP32-CAM and drawn
to the T-Display-S3's screen. With the **NEXT** and **PREV** on-screen buttons
the user can toggle between the options `COLOUR IMG`, `GRAY IMG`, and `NOT DISPLAYED`,
testing `CAMGet()`, `CAMGetGray()`, and `CAMGet()` not followed by a call to `LCDImage()`,
respectively.

### `VW SET SPEED`

This demo tests the `VWSetSpeed()` function, with the user able to set the linear
and angular speed parameters for the function, in mm/s and degrees/s respectively. 
The user can also set the PWM offsets
for the individual motors, with these values being passed directly through to the
`VWSetOffsets()` function. Each of the signs of these parameters can be flipped
by having the user touch the respective values themselves. When the user presses **START**, a single call to
`VWSetSpeed()` is made and the EyeBot will drive indefinitely until the user
touches **RESET** on the running screen, which also takes them back to the setting screen. On this running screen, the current estimate of the EyeBot's 
coordinates and orientation is printed to the screen.

### `VW STRAIGHT`

This demo tests the `VWStraight()` function, allowing the user to set the absolute
linear speed and the final change in distance the EyeBot will drive in a straight line
for. The sign of the distance parameter can be flipped by touching the value, and
these parameters are passed directly to the `VWStraight()` function. The user is
taken to the running screen after pressing **START**, which shows the current
estimate of the EyeBot's coordinates and orientation, as well as an estimation
of the remaining distance to be traveled for the finite operation. By touching **RESET** 
on this screen, the user can either cancel the `VWStraight()` operation if it is still
running, or just solely return to the settings screen if the operation has finished.

### `VW TURN`

This demo tests the `VWTurn()` function, allowing the user to set the absolute
angular speed and the final change in orientation for when the EyeBot will turn on
the spot. The sign of the change in orientation parameter (labelled `Final Ang. (deg)`)
can be flipped by the user touching the value. These parameter values are passed
directly through to `VWTurn()` when **START** is pressed, which takes the user to
the running screen. On the running screen, the current estimate for the EyeBot's
coordinates and orientation are printed to the screen. If the user touches
**RESET** on the running screen, it returns the user to the settings screen and
cancels the `VWTurn()` operation if it is still running.

### `VW CURVE`

This demo tests the `VWCurve()` function, with the user being able to set the
absolute linear speed, the desired change in orientation, and the desired
change in distance traveled. The user can flip the signs of the change in orientation
and distance values by touching the respective values. These parameters are
passed directly to the `VWCurve()` function when the user presses **START**
and is taken to the running screen. On the running screen the current estimate
of the EyeBot's coordinates and orientation is printed to the screen, along with
an estimate for the travel distance remaining for the operation. If the user touches
**RESET** on the running screen, the user is returned to the setting screen,
and the `VWCurve()` operation is cancelled if it is still running.

### `VW DRIVE`

This demo tests the `VWDrive()` function, with the user being able to set the
absolute linear speed, as well as the desired change in $x$ and $y$ coordinates
for the EyeBot to drive in a curve to. The signs of the desired change in $x$
and $y$ coordinates can be flipped by touching the respective values. The parameters
are passed directly to the `VWDrive()` when the user presses **START** and is
taken to the running screen. On the running screen the current estimate of the
EyeBot's coordinates and orientation are printed to the screen, along with an
estimate for the travel distance remaining for the operation. If the user touches
**RESET** on the running screen, the user is returned to the setting screen,
and the `VWDrive()` operation is cancelled if it is still running.
