# Colour-Based Navigation Program

With this program, the user can sample a colour in the view of the EyeBot's camera
and direct the EyeBot to drive towards the centre-point of the largest concentration
of pixels within the camera's view that fall within the desired threshold, stopping
before colliding into any objects head-on.

## Usage Instructions

### Home Screen

From here, the user can adjust the absolute linear and angular speed of the EyeBot
for when it enters the *Navigation Screen* after pressing the **START** button.
A preview box showing the currently selected pixel colour is positioned beside
the **RESELECT** button, which when pressed will take you to the *Colour Sampling Screen*.

### Colour Sampling Screen

The top section of this screen shows a current camera feed of the EyeBot, and beneath
this a preview box depicts the current average pixel colour beneath the red crosshairs
positioned in the centre of the camera image. When **SAMPLE** is pressed, the currently selected
pixel colour on the *Home Screen* is set to this average pixel colour.

Beneath the **SAMPLE** button, the user can adjust the plus-minus thresholds for the
hue, saturation, and intensity values of the currently selected colour. The user can
preview which pixels in the view of the camera fall within these thresholds for the
selected colour by clicking the physical right button. This button toggles a black and white
overlay for the camera feed which shows these pixels that fall within the threshold as white pixels,
and it can be reverted back to the raw camera feed by clicking the right button again.
Lastly, the user can go back to the *Home Screen* by clicking the left physical button.

### Navigation Screen

On the top of this screen there is a camera feed with red cross-hairs overlaid on it that
intersect over the row and column with the largest amount of pixels that fall within the
selected colour threshold. Below the camera feed the current distance read by the distance
sensor is printed to the screen, and a **COLLISION** warning appears beneath this value if it
falls below 100 mm. If the user touches any part of the screen,
the motors will be stopped and they will be returned back to *Home Screen*.