/**
 * Open Space Program
 * Copyright © 2019-2020 Open Space Program Project
 *
 * MIT License
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include <iostream>

#include <osp/Active/ActiveScene.h>
#include <osp/Active/physics.h>
#include <osp/Active/SysDebugRender.h>

#include "Rocket.h"
#include "osp/Resource/AssetImporter.h"
#include "adera/Shaders/Phong.h"
#include "adera/Shaders/PlumeShader.h"
#include "osp/Resource/blueprints.h"
#include "osp/PhysicsConstants.h"
#include "adera/SysExhaustPlume.h"
#include "adera/Plume.h"
#include <Magnum/Trade/MeshData.h>
#include <Magnum/Math/Color.h>
#include <Magnum/Math/Matrix4.h>

using namespace adera::active::machines;
using namespace osp::active;
using namespace osp;

void MachineRocket::propagate_output(WireOutput* output)
{

}

WireInput* MachineRocket::request_input(WireInPort port)
{
    return existing_inputs()[port];
}

WireOutput* MachineRocket::request_output(WireOutPort port)
{
    return existing_outputs()[port];
}

std::vector<WireInput*> MachineRocket::existing_inputs()
{
    /*std::vector<WireInput*> inputs;
    inputs.reserve(3 + m_resourceLines.size());

    inputs.insert(inputs.begin(), {&m_wiGimbal, &m_wiIgnition, &m_wiThrottle});
    for (auto& resource : m_resourceLines)
    {
        inputs.push_back(&resource.m_lineIn);
    }
    return inputs;*/
    return {&m_wiGimbal, &m_wiIgnition, &m_wiThrottle};
}

std::vector<WireOutput*> MachineRocket::existing_outputs()
{
    return {};
}

SysMachineRocket::SysMachineRocket(ActiveScene &rScene)
    : SysMachine<SysMachineRocket, MachineRocket>(rScene)
    , m_updatePhysics(rScene.get_update_order(), "mach_rocket", "controls", "physics",
        [this](ActiveScene& rScene) { this->update_physics(rScene); })
{

}

//void SysMachineRocket::update_sensor()
//{
//}

void SysMachineRocket::update_physics(ActiveScene& rScene)
{
    auto view = m_scene.get_registry().view<MachineRocket, ACompTransform>();

    for (ActiveEnt ent : view)
    {
        auto &machine = view.get<MachineRocket>(ent);
        if (!machine.m_enable)
        {
            continue;
        }

        // Check for nonzero throttle, continue otherwise
        WireData *pThrottle = machine.m_wiThrottle.connected_value();
        wiretype::Percent* pPercent = nullptr;
        if (pThrottle != nullptr)
        {
            using wiretype::Percent;
            pPercent = std::get_if<Percent>(pThrottle);
            if ((pPercent == nullptr) || !(pPercent->m_value > 0.0f))
            {
                continue;
            }
        }
        else
        {
            continue;
        }

        // Check for adequate resource inputs
        bool fail = false;
        for (auto& resource : machine.m_resourceLines)
        {
            auto const* src = m_scene.reg_try_get<MachineContainer>(resource.m_sourceEnt);
            if (!src)
            {
                std::cout << "Error: no source found\n";
                fail = true;
                break;
            }
            if (!src->check_contents().m_quantity > 0)
            {
                fail = true;
                break;
            }
        }
        if (fail)
        {
            continue;
        }

        // Perform physics calculation

        // Get rigidbody ancestor and its transformation component
        auto const* pRbAncestor =
            SysPhysics_t::try_get_or_find_rigidbody_ancestor(rScene, ent);
        auto& rCompRb = rScene.reg_get<ACompRigidBody_t>(pRbAncestor->m_ancestor);
        auto const& rCompTf = rScene.reg_get<ACompTransform>(pRbAncestor->m_ancestor);

        if (WireData *ignition = machine.m_wiIgnition.connected_value())
        {

        }

        Matrix4 relTransform = pRbAncestor->m_relTransform;

        /* Compute thrust force
            * Thrust force is defined to be along +Z by convention.
            * Obtains thrust vector in rigidbody space
            */
        Vector3 thrustDir = relTransform.transformVector(Vector3{0.0f, 0.0f, 1.0f});
        float thrustMag = machine.m_params.m_maxThrust * pPercent->m_value;
        // Take thrust in rigidbody space and apply to RB in world space
        Vector3 thrust = thrustMag * thrustDir;
        Vector3 worldThrust = rCompTf.m_transform.transformVector(thrust);
        SysPhysics_t::body_apply_force(rCompRb, worldThrust);

        // Obtain point where thrust is applied relative to RB CoM
        Vector3 location = relTransform.translation();
        // Compute worldspace torque from engine location, thrust vector
        Vector3 torque = Magnum::Math::cross(location, thrust);
        Vector3 worldTorque = rCompTf.m_transform.transformVector(torque);
        SysPhysics_t::body_apply_torque(rCompRb, worldTorque);
 
        // Perform resource consumption calculation
        float massFlowRateTot = thrustMag /
            (phys::constants::g_0 * machine.m_params.m_specImpulse);
        for (auto const& resource : machine.m_resourceLines)
        {
            float massFlowRate = massFlowRateTot * resource.m_massRateFraction;
            float massFlow = massFlowRate * m_scene.get_time_delta_fixed();
            uint64_t required = resource.m_type->resource_quantity(massFlow);
            auto* src = m_scene.reg_try_get<MachineContainer>(resource.m_sourceEnt);
            uint64_t consumed = src->request_contents(required);
            std::cout << "consumed " << consumed << " units of fuel, "
                << src->check_contents().m_quantity << " remaining\n";
        }
    }
}

void SysMachineRocket::attach_plume_effect(ActiveEnt ent)
{
    ActiveEnt plumeNode = entt::null;

    auto findPlumeHandle = [this, &plumeNode](ActiveEnt ent)
    {
        auto const& node = m_scene.reg_get<ACompHierarchy>(ent);
        static constexpr std::string_view nodePrefix = "fx_plume_";
        if (0 == node.m_name.compare(0, nodePrefix.size(), nodePrefix))
        {
            plumeNode = ent;
            return EHierarchyTraverseStatus::Stop;  // terminate search
        }
        return EHierarchyTraverseStatus::Continue;
    };

    m_scene.hierarchy_traverse(ent, findPlumeHandle);

    if (plumeNode == entt::null)
    {
        std::cout << "ERROR: could not find plume anchor for MachineRocket "
            << ent << "\n";
        return;
    }
    std::cout << "MachineRocket " << ent << "'s associated plume: "
        << plumeNode << "\n";

    // Get plume effect
    Package& pkg = m_scene.get_application().debug_find_package("lzdb");
    std::string_view plumeAnchorName = m_scene.reg_get<ACompHierarchy>(plumeNode).m_name;
    std::string_view effectName = plumeAnchorName.substr(3, plumeAnchorName.length() - 3);
    DependRes<PlumeEffectData> plumeEffect = pkg.get<PlumeEffectData>(effectName);
    if (plumeEffect.empty())
    {
        std::cout << "ERROR: couldn't find plume effect " << effectName << "!\n";
        return;
    }

    m_scene.reg_emplace<ACompExhaustPlume>(plumeNode, ent, plumeEffect);
}

Machine& SysMachineRocket::instantiate(ActiveEnt ent, PrototypeMachine config,
    BlueprintMachine settings)
{
    // Read engine config
    MachineRocket::Parameters params;
    params.m_maxThrust = std::get<double>(config.m_config["thrust"]);
    params.m_specImpulse = std::get<double>(config.m_config["Isp"]);

    MachineRocket::fuel_list_t inputs;
    Package& pkg = m_scene.get_application().debug_find_package("lzdb");
    DependRes<ShipResourceType> fuel = pkg.get<ShipResourceType>("fuel");
    if (!fuel.empty())
    {
        // TMP: finds the first non-empty fuel tank in the ship
        // Only works as long as the fuselage is added before the engine

        ActiveEnt foundTank = entt::null;
        auto find_fuel = [this, &foundTank](ActiveEnt e)
        {
            if (auto* fuel = this->m_scene.reg_try_get<MachineContainer>(e);
                fuel != nullptr && fuel->check_contents().m_quantity > 0)
            {
                foundTank = e;
                return EHierarchyTraverseStatus::Stop;
            }
            return EHierarchyTraverseStatus::Continue;
        };
        auto const& parent = m_scene.reg_get<ACompHierarchy>(ent);
        m_scene.hierarchy_traverse(parent.m_parent, find_fuel);
        // endTMP

        ActiveEnt fuelSource = foundTank;
        MachineRocket::ResourceInput input = {std::move(fuel), 1.0f, fuelSource};
        inputs.push_back(std::move(input));
    }

    attach_plume_effect(ent);
    return m_scene.reg_emplace<MachineRocket>(ent, params, inputs);
}

Machine& SysMachineRocket::get(ActiveEnt ent)
{
    return m_scene.reg_get<MachineRocket>(ent);//emplace(ent);
}
