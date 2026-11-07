// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Hidden Clay component holding the plugin catalogue JSON (plugins_catalog.ts, baked into
   clay-settings by PKJS seedPluginCatalog at showConfiguration). Renders nothing;
   config_plugin_list reads it via clayConfig.getItemByMessageKey('PluginCatalog').
   Serialized via toSource() -> keep self-contained. */

const pluginCatalogComponent = {
  name: 'pluginCatalog',
  template: '<div style="display:none"></div>',
  manipulator: {
    get: function(this: any): any { return this._v === undefined ? '' : this._v; },
    set: function(this: any, value: any) {
      let v: any = value;
      if (v && typeof v === 'object' && v.value !== undefined) { v = v.value; }
      this._v = v;
      return this;
    },
  },
  defaults: {},
};

export = pluginCatalogComponent;
