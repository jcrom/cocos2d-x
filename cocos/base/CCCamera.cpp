/****************************************************************************
 Copyright (c) 2014 Chukong Technologies Inc.
 
 http://www.cocos2d-x.org
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/
#include "base/CCCamera.h"
#include "base/CCDirector.h"
#include "platform/CCGLView.h"
#include "2d/CCScene.h"
#include "2d/CCNode.h"

NS_CC_BEGIN

Camera* Camera::_visitingCamera = nullptr;

Camera* Camera::create()
{
    Camera* camera = nullptr;
    auto size = Director::getInstance()->getWinSize();
    //create default camera
    auto projection = Director::getInstance()->getProjection();
    switch (projection)
    {
        case Director::Projection::_2D:
        {
            camera = Camera::createOrthographic(size.width, size.height, -1024, 1024);
            break;
        }
        case Director::Projection::_3D:
        {
            float zeye = Director::getInstance()->getZEye();
            camera = Camera::createPerspective(60, (GLfloat)size.width / size.height, 10, zeye + size.height / 2.0f);
            Vec3 eye(size.width/2, size.height/2.0f, zeye), center(size.width/2, size.height/2, 0.0f), up(0.0f, 1.0f, 0.0f);
            camera->setPosition3D(eye);
            camera->lookAt(center, up);
            break;
        }
        default:
            CCLOG("unrecognized projection");
            break;
    }
    return camera;
}

Camera* Camera::createPerspective(float fieldOfView, float aspectRatio, float nearPlane, float farPlane)
{
    auto ret = new Camera();
    if (ret)
    {
        ret->_fieldOfView = fieldOfView;
        ret->_aspectRatio = aspectRatio;
        ret->_nearPlane = nearPlane;
        ret->_farPlane = farPlane;
        Mat4::createPerspective(ret->_fieldOfView, ret->_aspectRatio, ret->_nearPlane, ret->_farPlane, &ret->_projection);
#if CC_TARGET_PLATFORM == CC_PLATFORM_WP8
        //if needed, we need to add a rotation for Landscape orientations on Windows Phone 8 since it is always in Portrait Mode
        GLView* view = Director::getInstance()->getOpenGLView();
        if(view != nullptr)
        {
            setAdditionalProjection(view->getOrientationMatrix());
        }
#endif
        ret->_viewProjectionDirty = true;
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

Camera* Camera::createOrthographic(float zoomX, float zoomY, float nearPlane, float farPlane)
{
    auto ret = new Camera();
    if (ret)
    {
        ret->_zoom[0] = zoomX;
        ret->_zoom[1] = zoomY;
        ret->_nearPlane = nearPlane;
        ret->_farPlane = farPlane;
        Mat4::createOrthographic(ret->_zoom[0], ret->_zoom[1], ret->_nearPlane, ret->_farPlane, &ret->_projection);
#if CC_TARGET_PLATFORM == CC_PLATFORM_WP8
        //if needed, we need to add a rotation for Landscape orientations on Windows Phone 8 since it is always in Portrait Mode
        GLView* view = Director::getInstance()->getOpenGLView();
        if(view != nullptr)
        {
            setAdditionalProjection(view->getOrientationMatrix());
        }
#endif
        ret->_viewProjectionDirty = true;
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

Camera::Camera()
: _cameraFlag(1)
, _scene(nullptr)
, _viewProjectionDirty(true)
{
    
}

Camera::~Camera()
{
    
}

void Camera::setPosition3D(const Vec3& position)
{
    Node::setPosition3D(position);
    
    _transformUpdated = _transformDirty = _inverseDirty = true;
}

const Mat4& Camera::getProjectionMatrix() const
{
    return _projection;
}
const Mat4& Camera::getViewMatrix() const
{
    Mat4 viewInv(getNodeToWorldTransform());
    static int count = sizeof(float) * 16;
    if (memcmp(viewInv.m, _viewInv.m, count) != 0)
    {
        _viewProjectionDirty = true;
        _viewInv = viewInv;
        _view = viewInv.getInversed();
    }
    return _view;
}
void Camera::lookAt(const Vec3& lookAtPos, const Vec3& up)
{
    Vec3 upv = up;
    upv.normalize();
    Vec3 zaxis;
    Vec3::subtract(this->getPosition3D(), lookAtPos, &zaxis);
    zaxis.normalize();
    
    Vec3 xaxis;
    Vec3::cross(upv, zaxis, &xaxis);
    xaxis.normalize();
    
    Vec3 yaxis;
    Vec3::cross(zaxis, xaxis, &yaxis);
    yaxis.normalize();
    Mat4  rotation;
    rotation.m[0] = xaxis.x;
    rotation.m[1] = xaxis.y;
    rotation.m[2] = xaxis.z;
    rotation.m[3] = 0;
    
    rotation.m[4] = yaxis.x;
    rotation.m[5] = yaxis.y;
    rotation.m[6] = yaxis.z;
    rotation.m[7] = 0;
    rotation.m[8] = zaxis.x;
    rotation.m[9] = zaxis.y;
    rotation.m[10] = zaxis.z;
    rotation.m[11] = 0;
    Quaternion  quaternion;
    Quaternion::createFromRotationMatrix(rotation,&quaternion);
    float fRoll  = atan2(2 * (quaternion.w * quaternion.z + quaternion.x * quaternion.y) , 1 - 2 * (quaternion.z * quaternion.z + quaternion.x * quaternion.x));
    float fPitch = asin(clampf(2 * (quaternion.w * quaternion.x - quaternion.y * quaternion.z) , -1.0f , 1.0f));
    float fYaw   = atan2(2 * (quaternion.w * quaternion.y + quaternion.z * quaternion.x) , 1 - 2 * (quaternion.x * quaternion.x + quaternion.y * quaternion.y));
    setRotation3D(Vec3(CC_RADIANS_TO_DEGREES(fPitch),CC_RADIANS_TO_DEGREES(fYaw),CC_RADIANS_TO_DEGREES(fRoll)));
}

const Mat4& Camera::getViewProjectionMatrix() const
{
    getViewMatrix();
    if (_viewProjectionDirty)
    {
        _viewProjectionDirty = false;
        Mat4::multiply(_projection, _view, &_viewProjection);
    }
    
    return _viewProjection;
}

void Camera::setAdditionalProjection(const Mat4& mat)
{
    _projection = mat * _projection;
    getViewProjectionMatrix();
}

void Camera::unproject(const Size& viewport, Vec3* src, Vec3* dst) const
{
    assert(dst);
    Vec4 screen(src->x / viewport.width, ((viewport.height - src->y)) / viewport.height, src->z, 1.0f);
    screen.x = screen.x * 2.0f - 1.0f;
    screen.y = screen.y * 2.0f - 1.0f;
    screen.z = screen.z * 2.0f - 1.0f;
    
    getViewProjectionMatrix().getInversed().transformVector(screen, &screen);
    if (screen.w != 0.0f)
    {
        screen.x /= screen.w;
        screen.y /= screen.w;
        screen.z /= screen.w;
    }
    
    dst->set(screen.x, screen.y, screen.z);
}

void Camera::onEnter()
{
    if (_scene == nullptr)
    {
        auto scene = getScene();
        if (scene)
            setScene(scene);
    }
    Node::onEnter();
}

void Camera::onExit()
{
    // remove this camera from scene
    setScene(nullptr);
    Node::onExit();
}

void Camera::setScene(Scene* scene)
{
    if (_scene != scene)
    {
        //remove old scene
        if (_scene)
        {
            auto& cameras = _scene->_cameras;
            auto it = std::find(cameras.begin(), cameras.end(), this);
            if (it != cameras.end())
                cameras.erase(it);
            _scene = nullptr;
        }
        //set new scene
        if (scene)
        {
            _scene = scene;
            auto& cameras = _scene->_cameras;
            auto it = std::find(cameras.begin(), cameras.end(), this);
            if (it == cameras.end())
                _scene->_cameras.push_back(this);
        }
    }
}

NS_CC_END
