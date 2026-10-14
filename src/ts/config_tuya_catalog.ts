// SPDX-License-Identifier: GPL-3.0-only
// Copyright (c) 2026 Tuomas Airaksinen

/* Hidden Clay component holding the Tuya catalog JSON (baked into clay-settings by
   PKJS seedTuyaCatalog). Renders nothing; config_tuya_list reads it via
   clayConfig.getItemByMessageKey('TuyaCatalog'). Serialized via toSource() -> keep
   self-contained. */

const tuyaCatalogComponent = {
  name: 'tuyaCatalog',
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

export = tuyaCatalogComponent;
