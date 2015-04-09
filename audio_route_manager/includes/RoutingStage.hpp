/*
 * Copyright (C) 2013-2015 Intel Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

namespace intel_audio
{

/**
 * Routing stage bits description.
 * It is used to feed the criteria of the Parameter Framework.
 * This criterion is inclusive and is used to encode
 * 5 steps of the routing (Muting, Disabling, Configuring, Enabling, Unmuting)
 * Muting -> EFlow
 * Disabling -> EPath
 * Configuring -> EConfigure
 * Enabling -> EConfigure | EPath
 * Unmuting -> EConfigure | EPath | EFlow
 */
enum RoutingStage
{
    Flow = 0,   /**< It refers to umute/unmute steps.   */
    Path,       /**< It refers to enable/disable steps  */
    Configure   /**< It refers to configure step        */
};

enum RoutingStageMask
{
    FlowMask = (1 << Flow),       /**< It refers to umute/unmute steps.   */
    PathMask = (1 << Path),       /**< It refers to enable/disable steps  */
    ConfigureMask = (1 << Configure)   /**< It refers to configure step        */
};

static const uint32_t gNbRoutingStages = 3;

} // namespace intel_audio
