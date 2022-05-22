#include "runtime/function/framework/component/camera/camera_component.h"

#include "runtime/core/base/macro.h"
#include "runtime/core/math/math_headers.h"

#include "runtime/function/character/character.h"
#include "runtime/function/framework/component/transform/transform_component.h"
#include "runtime/function/framework/level/level.h"
#include "runtime/function/framework/object/object.h"
#include "runtime/function/framework/world/world_manager.h"
#include "runtime/function/global/global_context.h"
#include "runtime/function/input/input_system.h"

#include "runtime/function/render/render_camera.h"

namespace Pilot
{
    RenderCamera* CameraComponent::m_render_camera = nullptr;

    void CameraComponent::postLoadResource(std::weak_ptr<GObject> parent_object)
    {
        m_parent_object = parent_object;

        const std::string& camera_type_name = m_camera_res.m_parameter.getTypeName();
        if (camera_type_name == "FirstPersonCameraParameter")
        {
            m_camera_mode = CameraMode::first_person;
        }
        else if (camera_type_name == "ThirdPersonCameraParameter")
        {
            m_camera_mode = CameraMode::third_person;
        }
        else if (camera_type_name == "FreeCameraParameter")
        {
            m_camera_mode = CameraMode::free;
        }
        else
        {
            LOG_ERROR("invalid camera type");
        }

        if (m_render_camera)
        {
            m_render_camera->setFOVx(m_camera_res.m_parameter->m_fov);
        }
    }

    void CameraComponent::tick(float delta_time)
    {
        if (!m_parent_object.lock())
            return;

        std::shared_ptr<Level>     current_level     = g_runtime_global_context.m_world_manager->getCurrentActiveLevel().lock();
        std::shared_ptr<Character> current_character = current_level->getCurrentActiveCharacter().lock();
        if (current_character == nullptr)
            return;

        if (current_character->getObjectID() != m_parent_object.lock()->getID())
            return;

        switch (m_camera_mode)
        {
            case CameraMode::first_person:
                tickFirstPersonCamera(delta_time);
                break;
            case CameraMode::third_person:
                tickThirdPersonCamera(delta_time);
                break;
            case CameraMode::free:
                break;
            default:
                break;
        }
    }

    void CameraComponent::tickFirstPersonCamera(float delta_time)
    {
        std::shared_ptr<Level>     current_level     = g_runtime_global_context.m_world_manager->getCurrentActiveLevel().lock();
        std::shared_ptr<Character> current_character = current_level->getCurrentActiveCharacter().lock();
        if (current_character == nullptr)
            return;

        Quaternion q_yaw, q_pitch;

        q_yaw.fromAngleAxis(g_runtime_global_context.m_input_system->m_cursor_delta_yaw, Vector3::UNIT_Z);
        q_pitch.fromAngleAxis(g_runtime_global_context.m_input_system->m_cursor_delta_pitch, m_left);

        const float offset  = static_cast<FirstPersonCameraParameter*>(m_camera_res.m_parameter)->m_vertical_offset;
        Vector3     eye_pos = current_character->getPosition() + offset * Vector3::UNIT_Z;

        m_foward = q_yaw * q_pitch * m_foward;
        m_left   = q_yaw * q_pitch * m_left;
        m_up     = m_foward.crossProduct(m_left);

        Matrix4x4 desired_mat = Math::makeLookAtMatrix(eye_pos, m_foward, m_up);

        if (m_render_camera)
        {
            m_render_camera->setMainViewMatrix(desired_mat, PCurrentCameraType::Motor);
        }

        Vector3    object_facing = m_foward - m_foward.dotProduct(Vector3::UNIT_Z) * Vector3::UNIT_Z;
        Vector3    object_left   = Vector3::UNIT_Z.crossProduct(object_facing);
        Quaternion object_rotation;
        object_rotation.fromAxes(object_left, -object_facing, Vector3::UNIT_Z);
        current_character->setRotation(object_rotation);
    }

    void CameraComponent::tickThirdPersonCamera(float delta_time)
    {
        std::shared_ptr<Level>     current_level     = g_runtime_global_context.m_world_manager->getCurrentActiveLevel().lock();
        std::shared_ptr<Character> current_character = current_level->getCurrentActiveCharacter().lock();
        if (current_character == nullptr)
            return;

        ThirdPersonCameraParameter* param = static_cast<ThirdPersonCameraParameter*>(m_camera_res.m_parameter);

        Quaternion q_yaw, q_pitch;

        q_yaw.fromAngleAxis(g_runtime_global_context.m_input_system->m_cursor_delta_yaw, Vector3::UNIT_Z);
        q_pitch.fromAngleAxis(g_runtime_global_context.m_input_system->m_cursor_delta_pitch, Vector3::UNIT_X);

        param->m_cursor_pitch = q_pitch * param->m_cursor_pitch;

        const float vertical_offset   = param->m_vertical_offset;
        const float horizontal_offset = param->m_horizontal_offset;
        Vector3     offset            = Vector3(0, horizontal_offset, vertical_offset);

        Vector3 center_pos = current_character->getPosition() + Vector3::UNIT_Z * vertical_offset;
        Vector3 camera_pos =
            current_character->getRotation() * param->m_cursor_pitch * offset + current_character->getPosition();
        Vector3 camera_forward = center_pos - camera_pos;
        Vector3 camera_up      = current_character->getRotation() * param->m_cursor_pitch * Vector3::UNIT_Z;

        current_character->setRotation(q_yaw * current_character->getRotation());

        Matrix4x4 desired_mat = Math::makeLookAtMatrix(camera_pos, camera_pos + camera_forward, camera_up);

        if (m_render_camera)
        {
            m_render_camera->setMainViewMatrix(desired_mat, PCurrentCameraType::Motor);
        }
    }
} // namespace Pilot
