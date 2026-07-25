/* foundry-ci-manager.h
 *
 * Copyright 2026 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include "foundry-ci-run-options.h"
#include "foundry-service.h"
#include "foundry-types.h"
#include "foundry-version-macros.h"

G_BEGIN_DECLS

#define FOUNDRY_TYPE_CI_MANAGER (foundry_ci_manager_get_type())

FOUNDRY_AVAILABLE_IN_1_2
FOUNDRY_DECLARE_INTERNAL_TYPE (FoundryCiManager, foundry_ci_manager, FOUNDRY, CI_MANAGER, FoundryService)

FOUNDRY_AVAILABLE_IN_1_2
DexFuture *foundry_ci_manager_list_pipelines (FoundryCiManager    *self) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture *foundry_ci_manager_run            (FoundryCiManager    *self,
                                              FoundryCiPipeline   *pipeline,
                                              const char * const  *job_ids,
                                              FoundryCiRunOptions *options) G_GNUC_WARN_UNUSED_RESULT;
FOUNDRY_AVAILABLE_IN_1_2
DexFuture *foundry_ci_manager_run_shell      (FoundryCiManager    *self,
                                              FoundryCiJob        *job,
                                              FoundryCiRunOptions *options) G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS
