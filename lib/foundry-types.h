/* foundry-types.h
 *
 * Copyright 2024 Christian Hergert <chergert@redhat.com>
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

#include <glib.h>

G_BEGIN_DECLS

/*<private>
 * FOUNDRY_DECLARE_INTERNAL_TYPE:
 * @ModuleObjName: The name of the new type, in camel case (like GtkWidget)
 * @module_obj_name: The name of the new type in lowercase, with words
 *  separated by '_' (like 'gtk_widget')
 * @MODULE: The name of the module, in all caps (like 'GTK')
 * @OBJ_NAME: The bare name of the type, in all caps (like 'WIDGET')
 * @ParentName: the name of the parent type, in camel case (like GtkWidget)
 *
 * A convenience macro for emitting the usual declarations in the
 * header file for a type which is intended to be subclassed only
 * by internal consumers.
 *
 * This macro differs from %G_DECLARE_DERIVABLE_TYPE and %G_DECLARE_FINAL_TYPE
 * by declaring a type that is only derivable internally. Internal users can
 * derive this type, assuming they have access to the instance and class
 * structures; external users will not be able to subclass this type.
 */
#define FOUNDRY_DECLARE_INTERNAL_TYPE(ModuleObjName, module_obj_name, MODULE, OBJ_NAME, ParentName)   \
  GType module_obj_name##_get_type (void);                                                            \
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS                                                                    \
  typedef struct _##ModuleObjName ModuleObjName;                                                      \
  typedef struct _##ModuleObjName##Class ModuleObjName##Class;                                        \
                                                                                                      \
  _GLIB_DEFINE_AUTOPTR_CHAINUP (ModuleObjName, ParentName)                                            \
  G_DEFINE_AUTOPTR_CLEANUP_FUNC (ModuleObjName##Class, g_type_class_unref)                            \
                                                                                                      \
  G_GNUC_UNUSED static inline ModuleObjName * MODULE##_##OBJ_NAME (gpointer ptr) {                    \
    return G_TYPE_CHECK_INSTANCE_CAST (ptr, module_obj_name##_get_type (), ModuleObjName); }          \
  G_GNUC_UNUSED static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_CLASS (gpointer ptr) {     \
    return G_TYPE_CHECK_CLASS_CAST (ptr, module_obj_name##_get_type (), ModuleObjName##Class); }      \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME (gpointer ptr) {                        \
    return G_TYPE_CHECK_INSTANCE_TYPE (ptr, module_obj_name##_get_type ()); }                         \
  G_GNUC_UNUSED static inline gboolean MODULE##_IS_##OBJ_NAME##_CLASS (gpointer ptr) {                \
    return G_TYPE_CHECK_CLASS_TYPE (ptr, module_obj_name##_get_type ()); }                            \
  G_GNUC_UNUSED static inline ModuleObjName##Class * MODULE##_##OBJ_NAME##_GET_CLASS (gpointer ptr) { \
    return G_TYPE_INSTANCE_GET_CLASS (ptr, module_obj_name##_get_type (), ModuleObjName##Class); }    \
  G_GNUC_END_IGNORE_DEPRECATIONS

typedef struct _FoundryBuildAddin         FoundryBuildAddin;
typedef struct _FoundryBuildFlags         FoundryBuildFlags;
typedef struct _FoundryBuildManager       FoundryBuildManager;
typedef struct _FoundryBuildPipeline      FoundryBuildPipeline;
typedef struct _FoundryBuildProgress      FoundryBuildProgress;
typedef struct _FoundryBuildStage         FoundryBuildStage;
typedef struct _FoundryCliCommand         FoundryCliCommand;
typedef struct _FoundryCliTool            FoundryCliTool;
typedef struct _FoundryCodeAction         FoundryCodeAction;
typedef struct _FoundryCommandLine        FoundryCommandLine;
typedef struct _FoundryCommand            FoundryCommand;
typedef struct _FoundryCommandManager     FoundryCommandManager;
typedef struct _FoundryCommandProvider    FoundryCommandProvider;
typedef struct _FoundryCommandStage       FoundryCommandStage;
typedef struct _FoundryCompileCommands    FoundryCompileCommands;
typedef struct _FoundryConfig             FoundryConfig;
typedef struct _FoundryConfigManager      FoundryConfigManager;
typedef struct _FoundryConfigProvider     FoundryConfigProvider;
typedef struct _FoundryContext            FoundryContext;
typedef struct _FoundryContextual         FoundryContextual;
typedef struct _FoundryDBusService        FoundryDBusService;
typedef struct _FoundryDebugManager       FoundryDebugManager;
typedef struct _FoundryDebugger           FoundryDebugger;
typedef struct _FoundryDeployStrategy     FoundryDeployStrategy;
typedef struct _FoundryDevice             FoundryDevice;
typedef enum   _FoundryDeviceChassis      FoundryDeviceChassis;
typedef struct _FoundryDeviceInfo         FoundryDeviceInfo;
typedef struct _FoundryDeviceProvider     FoundryDeviceProvider;
typedef struct _FoundryDeviceManager      FoundryDeviceManager;
typedef struct _FoundryDiagnostic         FoundryDiagnostic;
typedef struct _FoundryDiagnosticBuilder  FoundryDiagnosticBuilder;
typedef struct _FoundryDiagnosticProvider FoundryDiagnosticProvider;
typedef struct _FoundryDiagnosticTool     FoundryDiagnosticTool;
typedef struct _FoundryDiagnosticManager  FoundryDiagnosticManager;
typedef struct _FoundryDirectoryReaper    FoundryDirectoryReaper;
typedef struct _FoundryExtension          FoundryExtension;
typedef struct _FoundryExtensionSet       FoundryExtensionSet;
typedef struct _FoundryFileManager        FoundryFileManager;
typedef struct _FoundryFutureListModel    FoundryFutureListModel;
typedef struct _FoundryLogManager         FoundryLogManager;
typedef struct _FoundryLogMessage         FoundryLogMessage;
typedef struct _FoundryLspClient          FoundryLspClient;
typedef struct _FoundryLspManager         FoundryLspManager;
typedef struct _FoundryLspProvider        FoundryLspProvider;
typedef struct _FoundryMarkup             FoundryMarkup;
typedef struct _FoundryOperation          FoundryOperation;
typedef struct _FoundryOperationManager   FoundryOperationManager;
typedef struct _FoundryPathCache          FoundryPathCache;
typedef struct _FoundryPipeline           FoundryPipeline;
typedef struct _FoundryProcessLauncher    FoundryProcessLauncher;
typedef struct _FoundryRunManager         FoundryRunManager;
typedef struct _FoundryRunTool            FoundryRunTool;
typedef struct _FoundrySdk                FoundrySdk;
typedef struct _FoundrySdkManager         FoundrySdkManager;
typedef struct _FoundrySdkProvider        FoundrySdkProvider;
typedef struct _FoundrySettings           FoundrySettings;
typedef struct _FoundrySymbol             FoundrySymbol;
typedef struct _FoundrySymbolProvider     FoundrySymbolProvider;
typedef struct _FoundrySearchManager      FoundrySearchManager;
typedef struct _FoundryService            FoundryService;
typedef struct _FoundryTextDocument       FoundryTextDocument;
typedef struct _FoundryTextEdit           FoundryTextEdit;
typedef struct _FoundryTextManager        FoundryTextManager;
typedef struct _FoundryTriplet            FoundryTriplet;
typedef struct _FoundryUnixFDMap          FoundryUnixFDMap;
typedef struct _FoundryVcs                FoundryVcs;
typedef struct _FoundryVcsManager         FoundryVcsManager;
typedef struct _FoundryVcsProvider        FoundryVcsProvider;

typedef enum _FoundryBuildPipelinePhase
{
  FOUNDRY_BUILD_PIPELINE_PHASE_NONE         = 0,
  FOUNDRY_BUILD_PIPELINE_PHASE_PREPARE      = 1 << 0,
  FOUNDRY_BUILD_PIPELINE_PHASE_DOWNLOADS    = 1 << 1,
  FOUNDRY_BUILD_PIPELINE_PHASE_DEPENDENCIES = 1 << 2,
  FOUNDRY_BUILD_PIPELINE_PHASE_AUTOGEN      = 1 << 3,
  FOUNDRY_BUILD_PIPELINE_PHASE_CONFIGURE    = 1 << 4,
  FOUNDRY_BUILD_PIPELINE_PHASE_BUILD        = 1 << 6,
  FOUNDRY_BUILD_PIPELINE_PHASE_INSTALL      = 1 << 7,
  FOUNDRY_BUILD_PIPELINE_PHASE_COMMIT       = 1 << 8,
  FOUNDRY_BUILD_PIPELINE_PHASE_EXPORT       = 1 << 9,
  FOUNDRY_BUILD_PIPELINE_PHASE_FINAL        = 1 << 10,

  FOUNDRY_BUILD_PIPELINE_PHASE_BEFORE       = 1 << 28,
  FOUNDRY_BUILD_PIPELINE_PHASE_AFTER        = 1 << 29,
  FOUNDRY_BUILD_PIPELINE_PHASE_FINISHED     = 1 << 30,
  FOUNDRY_BUILD_PIPELINE_PHASE_FAILED       = 1 << 31,
} FoundryBuildPipelinePhase;

G_END_DECLS
