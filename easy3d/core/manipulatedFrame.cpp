/****************************************************************************

 Copyright (C) 2002-2014 Gilles Debunne. All rights reserved.

 This file is part of the QGLViewer library version 2.7.1.

 http://www.libqglviewer.com - contact@libqglviewer.com

 This file may be used under the terms of the GNU General Public License
 versions 2.0 or 3.0 as published by the Free Software Foundation and
 appearing in the LICENSE file included in the packaging of this file.
 In addition, as a special exception, Gilles Debunne gives you certain
 additional rights, described in the file GPL_EXCEPTION in this package.

 libQGLViewer uses dual licensing. Commercial/proprietary software must
 purchase a libQGLViewer Commercial License.

 This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.

*****************************************************************************/

#include "manipulatedFrame.h"
#include "camera.h"
#include "manipulatedCameraFrame.h"

#include <cstdlib>
#include <GLFW/glfw3.h> // Include glfw3.h after our OpenGL definitions


namespace easy3d {

	/*! Default constructor.

	  The translation is set to (0,0,0), with an identity rotation (0,0,0,1) (see
	  Frame constructor for details).

	  The different sensitivities are set to their default values (see
	  rotationSensitivity(), translationSensitivity(), spinningSensitivity() and
	  wheelSensitivity()). */
	ManipulatedFrame::ManipulatedFrame()
	{
		// #CONNECTION# initFromDOMElement and accessor docs
		setRotationSensitivity(1.0);
		setTranslationSensitivity(1.0);
		setWheelSensitivity(1.0);
		setZoomSensitivity(1.0);

		previousConstraint_ = NULL;
	}

	/*! Equal operator. Calls Frame::operator=() and then copy attributes. */
	ManipulatedFrame &ManipulatedFrame::operator=(const ManipulatedFrame &mf) {
		Frame::operator=(mf);

		setRotationSensitivity(mf.rotationSensitivity());
		setTranslationSensitivity(mf.translationSensitivity());
		setWheelSensitivity(mf.wheelSensitivity());
		setZoomSensitivity(mf.zoomSensitivity());

		dirIsFixed_ = false;

		return *this;
	}

	/*! Copy constructor. Performs a deep copy of all attributes using operator=().
	 */
	ManipulatedFrame::ManipulatedFrame(const ManipulatedFrame &mf)
		: Frame(mf)
	{
		(*this) = mf;
	}

	////////////////////////////////////////////////////////////////////////////////
	//                 M o u s e    h a n d l i n g                               //
	////////////////////////////////////////////////////////////////////////////////

	/*! Rotates the ManipulatedFrame by its spinningQuaternion(). Called by a timer
	  when the ManipulatedFrame isSpinning(). */
	void ManipulatedFrame::spin() { rotate(spinningQuaternion()); }

	/*! Return 1 if mouse motion was started horizontally and -1 if it was more
	vertical. Returns 0 if this could not be determined yet (perfect diagonal
	motion, rare). */
	int ManipulatedFrame::mouseOriginalDirection(int x, int y, int dx, int dy) {
		static bool horiz = true; // Two simultaneous manipulatedFrame require two mice !

		if (!dirIsFixed_) {
			dirIsFixed_ = abs(dx) != abs(dy);
			horiz = abs(dx) > abs(dy);
		}

		if (dirIsFixed_)
			if (horiz)
				return 1;
			else
				return -1;
		else
			return 0;
	}

	float ManipulatedFrame::deltaWithPrevPos(int x, int y, int dx, int dy, Camera *const camera) const {
		float delta_x = float(dx) / camera->screenWidth();
		float delta_y = float(dy) / camera->screenHeight();

		float value = fabs(delta_x) > fabs(delta_y) ? delta_x : delta_y;
		return value * zoomSensitivity();
	}

	float ManipulatedFrame::wheelDelta(int x, int y, int dx, int dy) const {
		(void)x; (void)y; (void)dx;
		static const float WHEEL_SENSITIVITY_COEF = 0.1f;
		return dy * wheelSensitivity() * WHEEL_SENSITIVITY_COEF;
	}

	void ManipulatedFrame::zoom(float delta, const Camera *const camera) {
		vec3 trans(0.0, 0.0, (camera->position() - position()).norm() * delta);

		trans = camera->frame()->orientation().rotate(trans);
		if (referenceFrame())
			trans = referenceFrame()->transformOf(trans);
		translate(trans);
	}


	/*! Initiates the ManipulatedFrame mouse manipulation.

	Overloading of MouseGrabber::mousePressEvent(). See also mouseMoveEvent() and
	mouseReleaseEvent().

	The mouse behavior depends on which button is pressed. See the <a
	href="../mouse.html">QGLViewer mouse page</a> for details. */
	void ManipulatedFrame::mousePressEvent(int x, int y, int button, int modifiers, Camera *const camera) {
		(void)x;
		(void)y;
		(void)button;
		(void)modifiers;
		(void)camera;
		if (modifiers == GLFW_MOD_SHIFT && button == GLFW_MOUSE_BUTTON_RIGHT)  // SCREEN_TRANSLATE
			dirIsFixed_ = false;
	}


	/*! Modifies the ManipulatedFrame according to the mouse motion.

	Actual behavior depends on mouse bindings. See the QGLViewer::MouseAction enum
	and the <a href="../mouse.html">QGLViewer mouse page</a> for details.

	The \p camera is used to fit the mouse motion with the display parameters (see
	Camera::screenWidth(), Camera::screenHeight(), Camera::fieldOfView()).

	Emits the manipulated() signal. */
	void ManipulatedFrame::mouseMoveEvent(int x, int y, int dx, int dy, int button, int modifiers, Camera *const camera)
	{
		if (modifiers == 0 && button == GLFW_MOUSE_BUTTON_LEFT)	// QGLViewer::ROTATE
		{
			vec3 trans = camera->projectedCoordinatesOf(position());
			int pre_x = x - dx;
			int pre_y = y - dy;
			quat rot = deformedBallQuaternion(x, y, pre_x, pre_y, trans[0], trans[1], camera);
			trans = vec3(-rot[0], -rot[1], -rot[2]);
			trans = camera->frame()->orientation().rotate(trans);
			trans = transformOf(trans);
			rot[0] = trans[0];
			rot[1] = trans[1];
			rot[2] = trans[2];
			setSpinningQuaternion(rot);
			spin();
		}
		else if (modifiers == 0 && button == GLFW_MOUSE_BUTTON_RIGHT)	// QGLViewer::TRANSLATE
		{
			vec3 trans(float(dx), float(-dy), 0.0f);
			// Scale to fit the screen mouse displacement
			switch (camera->type())
			{
			case Camera::PERSPECTIVE:
				trans *= 2.0 * tan(camera->fieldOfView() / 2.0) *
					fabs((camera->frame()->coordinatesOf(position())).z) /
					camera->screenHeight();
				break;
			case Camera::ORTHOGRAPHIC: {
				float w, h;
				camera->getOrthoWidthHeight(w, h);
				trans[0] *= 2.0f * float(w) / camera->screenWidth();
				trans[1] *= 2.0f * float(h) / camera->screenHeight();
				break;
			}
			}
			// Transform to world coordinate system.
			trans =
				camera->frame()->orientation().rotate(translationSensitivity() * trans);
			// And then down to frame
			if (referenceFrame())
				trans = referenceFrame()->transformOf(trans);
			translate(trans);
		}

		else if (modifiers == GLFW_MOD_SHIFT && button == GLFW_MOUSE_BUTTON_LEFT) // SCREEN_ROTATE
		{
			vec3 trans = camera->projectedCoordinatesOf(position());

			float pre_x = float(x - dx);
			float pre_y = float(y - dy);
			const float prev_angle = atan2(pre_y - trans[1], pre_x - trans[0]);
			const float angle = atan2(y - trans[1], x - trans[0]);

			const vec3 axis = transformOf(camera->frame()->inverseTransformOf(vec3(0.0, 0.0, -1.0)));
			quat rot(axis, angle - prev_angle);
			setSpinningQuaternion(rot);
			spin();
		}

		else if (modifiers == GLFW_MOD_SHIFT && button == GLFW_MOUSE_BUTTON_RIGHT)  // SCREEN_TRANSLATE
		{
			vec3 trans;
			int dir = mouseOriginalDirection(x, y, dx, dy);
			if (dir == 1)
				trans = vec3(float(dx), 0.0f, 0.0f);
			else if (dir == -1)
				trans = vec3(0.0f, float(-dy), 0.0f);

			switch (camera->type()) {
			case Camera::PERSPECTIVE:
				trans *= 2.0 * tan(camera->fieldOfView() / 2.0f) *
					fabs((camera->frame()->coordinatesOf(position())).z) /
					camera->screenHeight();
				break;
			case Camera::ORTHOGRAPHIC: {
				float w, h;
				camera->getOrthoWidthHeight(w, h);
				trans[0] *= 2.0f * float(w) / camera->screenWidth();
				trans[1] *= 2.0f * float(h) / camera->screenHeight();
				break;
			}
			}
			// Transform to world coordinate system.
			trans =
				camera->frame()->orientation().rotate(translationSensitivity() * trans);
			// And then down to frame
			if (referenceFrame())
				trans = referenceFrame()->transformOf(trans);

			translate(trans);
		}

		frameModified();
	}

	/*! Stops the ManipulatedFrame mouse manipulation.

	Overloading of MouseGrabber::mouseReleaseEvent().

	If the action was a QGLViewer::ROTATE QGLViewer::MouseAction, a continuous
	spinning is possible if the speed of the mouse cursor is larger than
	spinningSensitivity() when the button is released. Press the rotate button again
	to stop spinning. See startSpinning() and isSpinning(). */
	void ManipulatedFrame::mouseReleaseEvent(int x, int y, int button, int modifiers, Camera *const camera)
	{
		(void)x; (void)y; (void)button; (void)modifiers; (void)camera;
		if (previousConstraint_)
			setConstraint(previousConstraint_);
	}

	/*! Overloading of MouseGrabber::mouseDoubleClickEvent().

	Left button double click aligns the ManipulatedFrame with the \p camera axis
	(see alignWithFrame() and QGLViewer::ALIGN_FRAME). Right button projects the
	ManipulatedFrame on the \p camera view direction. */
	void ManipulatedFrame::mouseDoubleClickEvent(int x, int y, int button, int modifiers, Camera *const camera) {
		if (modifiers == 0)
			switch (button)
			{
			case GLFW_MOUSE_BUTTON_LEFT:
				alignWithFrame(camera->frame());
				break;
			case GLFW_MOUSE_BUTTON_RIGHT:
				projectOnLine(camera->position(), camera->viewDirection());
				break;
			default:
				break;
			}
	}

	/*! Overloading of MouseGrabber::wheelEvent().

	Using the wheel is equivalent to a QGLViewer::ZOOM QGLViewer::MouseAction. See
	 QGLViewer::setWheelBinding(), setWheelSensitivity(). */
	void ManipulatedFrame::wheelEvent(int x, int y, int dx, int dy, Camera *const camera) {
		zoom(wheelDelta(x, y, dx, dy), camera);
		frameModified();

		// #CONNECTION# startAction should always be called before
		if (previousConstraint_)
			setConstraint(previousConstraint_);
	}

	////////////////////////////////////////////////////////////////////////////////

	/*! Returns "pseudo-distance" from (x,y) to ball of radius size.
	\arg for a point inside the ball, it is proportional to the euclidean distance
	to the ball \arg for a point outside the ball, it is proportional to the inverse
	of this distance (tends to zero) on the ball, the function is continuous. */
	static float projectOnBall(float x, float y) {
		// If you change the size value, change angle computation in
		// deformedBallQuaternion().
		const float size = 1.0;
		const float size2 = size * size;
		const float size_limit = size2 * 0.5;

		const float d = x * x + y * y;
		return d < size_limit ? sqrt(size2 - d) : size_limit / sqrt(d);
	}

#ifndef DOXYGEN
	/*! Returns a quaternion computed according to the mouse motion. Mouse positions
	are projected on a deformed ball, centered on (\p cx,\p cy). */
	quat ManipulatedFrame::deformedBallQuaternion(int x, int y, int pre_x, int pre_y, float cx, float cy, const Camera *const camera) {
		// Points on the deformed ball
		float px = rotationSensitivity() * (pre_x - cx) / camera->screenWidth();
		float py = rotationSensitivity() * (cy - pre_y) / camera->screenHeight();
		float dx = rotationSensitivity() * (x - cx) / camera->screenWidth();
		float dy = rotationSensitivity() * (cy - y) / camera->screenHeight();

		const vec3 p1(px, py, projectOnBall(px, py));
		const vec3 p2(dx, dy, projectOnBall(dx, dy));
		// Approximation of rotation angle
		// Should be divided by the projectOnBall size, but it is 1.0
		const vec3 axis = cross(p2, p1);
		const float angle = 5.0f * asin(sqrt(axis.length2() / p1.length2() / p2.length2()));
		return quat(axis, angle);
	}

}


#endif // DOXYGEN
