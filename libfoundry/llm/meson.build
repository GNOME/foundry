libfoundry_config_conf.set10('FOUNDRY_FEATURE_LLM', true)

foundry_sources += files([
  'foundry-llm-chat.c',
  'foundry-llm-chat-message.c',
  'foundry-llm-completion.c',
  'foundry-llm-completion-chunk.c',
  'foundry-llm-completion-params.c',
  'foundry-llm-manager.c',
  'foundry-llm-model.c',
  'foundry-llm-provider.c',
  'foundry-llm-tool.c',
])

foundry_headers += files([
  'foundry-llm.h',
  'foundry-llm-chat.h',
  'foundry-llm-chat-message.h',
  'foundry-llm-completion.h',
  'foundry-llm-completion-chunk.h',
  'foundry-llm-completion-params.h',
  'foundry-llm-manager.h',
  'foundry-llm-model.h',
  'foundry-llm-provider.h',
  'foundry-llm-tool.h',
])

foundry_include_directories += [include_directories('.')]
