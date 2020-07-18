#include <iostream>

#include <osp/Active/ActiveScene.h>

#include "UserControl.h"

namespace osp
{

MachineUserControl::MachineUserControl() :
    Machine(false),
    //m_woTestPropagate(this, "TestOut", &MachineUserControl::propagate_test),
    m_woAttitude(this, "AttitudeControl"),
    m_woTestPropagate(this, "TestOut", m_wiTest),
    m_woThrottle(this, "Throttle"),
    m_wiTest(this, "Test")
{
    //m_woTestPropagate.propagate();
    //m_enable = true;
    m_woAttitude.value() = wiretype::AttitudeControl{};
    m_woThrottle.value() = wiretype::Percent{0.0f};
}

MachineUserControl::MachineUserControl(MachineUserControl&& move) :
    Machine(std::move(move)),
    m_woAttitude(this, std::move(move.m_woAttitude)),
    m_woTestPropagate(this, std::move(move.m_woTestPropagate)),
    m_woThrottle(this, std::move(move.m_woThrottle)),
    m_wiTest(this, std::move(move.m_wiTest))
{
    //m_enable = true;
    //_woThrottle.value() = wiretype::Percent{0.0f};
}

MachineUserControl& MachineUserControl::operator=(MachineUserControl&& move)
{
    m_enable = move.m_enable;
}

void MachineUserControl::propagate_output(WireOutput* output)
{
    std::cout << "propagate test: " << output->get_name() << "\n";
}

WireInput* MachineUserControl::request_input(WireInPort port)
{
    return existing_inputs()[port];
}

WireOutput* MachineUserControl::request_output(WireOutPort port)
{
    return existing_outputs()[port];
}

std::vector<WireInput*> MachineUserControl::existing_inputs()
{
    return {&m_wiTest};
}

std::vector<WireOutput*> MachineUserControl::existing_outputs()
{
    return {&m_woAttitude, &m_woThrottle, &m_woTestPropagate};
}

SysMachineUserControl::SysMachineUserControl(ActiveScene &scene, UserInputHandler& userControl) :
        SysMachine<SysMachineUserControl, MachineUserControl>(scene),
        m_throttleMax(userControl.config_get("game_thr_max")),
        m_throttleMin(userControl.config_get("game_thr_min")),
        m_selfDestruct(userControl.config_get("game_self_destruct")),
        m_pitchUp(userControl.config_get("game_pitch_up")),
        m_pitchDn(userControl.config_get("game_pitch_dn")),
        m_yawLf(userControl.config_get("game_yaw_lf")),
        m_yawRt(userControl.config_get("game_yaw_rt")),
        m_rollLf(userControl.config_get("game_roll_lf")),
        m_rollRt(userControl.config_get("game_roll_rt")),
        m_updateSensor(scene.get_update_order(), "mach_usercontrol", "", "wire",
                       std::bind(&SysMachineUserControl::update_sensor, this))
{

}

void SysMachineUserControl::update_sensor()
{
    //std::cout << "updating all MachineUserControls\n";
    // InputDevice.IsActivated()
    // Combination
    

    if (m_selfDestruct.triggered())
    {
        std::cout << "EXPLOSION BOOM!!!!\n";
    }

    // pitch, yaw, roll
    Vector3 attitudeIn(
            m_pitchDn.trigger_hold() - m_pitchUp.trigger_hold(),
            m_yawLf.trigger_hold() - m_yawRt.trigger_hold(),
            m_rollRt.trigger_hold() - m_rollLf.trigger_hold());

    auto view = m_scene.get_registry().view<MachineUserControl>();

    for (ActiveEnt ent : view)
    {
        MachineUserControl &machine = view.get<MachineUserControl>(ent);


        if (!machine.m_enable)
        {
            continue;
        }

        if (m_throttleMin.triggered())
        {
            //std::cout << "throttle min\n";
            std::get<wiretype::Percent>(machine.m_woThrottle.value()).m_value = 0.0f;
        }

        if (m_throttleMax.triggered())
        {
            //std::cout << "throttle max\n";
            std::get<wiretype::Percent>(machine.m_woThrottle.value()).m_value = 1.0f;
        }

        std::get<wiretype::AttitudeControl>(machine.m_woAttitude.value()).m_attitude = attitudeIn;
        //std::cout << "updating control\n";
    }
}



Machine& SysMachineUserControl::instantiate(ActiveEnt ent)
{
    return m_scene.reg_emplace<MachineUserControl>(ent);
}


Machine& SysMachineUserControl::get(ActiveEnt ent)
{
    return m_scene.reg_get<MachineUserControl>(ent);
}

}
