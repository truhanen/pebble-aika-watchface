// Ambient declarations for the phone-side plugin API (Pebble SDK 4.44+). Both
// methods are optional: an older phone app lacks them, so callers feature-detect.

interface PebbleNumericValue {
  value: number;
  unit?: string;
}

interface PebblePluginProperty {
  shortText?: { text?: string };
  numericValue?: PebbleNumericValue;
  [shape: string]: any;
}

interface PebblePluginInstance {
  instanceId?: string;
  properties?: Record<string, PebblePluginProperty>;
}

interface PebblePluginEnvelope {
  pluginUuid?: string;
  instances?: PebblePluginInstance[];
}

interface PebbleSourceSubscription {
  unsubscribe(): void;
}

interface PebbleSourceRequest {
  plugin?: string;
  category: string;
  item: string;
  properties?: string[];
  iconPixelSize?: { w: number; h: number };   // only needed when an icon property is read
  onData: (envelope: PebblePluginEnvelope) => void;
  onError?: (err: any) => void;
}

interface Pebble {
  enumeratePlugins?(): any[];
  subscribeToSource?(req: PebbleSourceRequest): PebbleSourceSubscription;
}
