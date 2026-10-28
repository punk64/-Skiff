export default function deepClone<T>(source: T): T {
  // 处理原始类型和null/undefined
  if (source === null || typeof source !== 'object') {
    return source;
  }

  // 处理日期对象
  if (source instanceof Date) {
    return new Date(source.getTime()) as T;
  }

  // 处理数组
  if (Array.isArray(source)) {
    const cloneArray: any[] = [];
    for (const item of source) {
      cloneArray.push(deepClone(item));
    }
    return cloneArray as T;
  }

  // 处理普通对象
  if (Object.prototype.toString.call(source) === '[object Object]') {
    const cloneObject: Record<string, any> = {};
    const keys = Object.keys(source);
    for (const key of keys) {
      if (source.hasOwnProperty(key)) {
        cloneObject[key] = deepClone((source as any)[key]);
      }
    }
    return cloneObject as T;
  }
  return source;
}
