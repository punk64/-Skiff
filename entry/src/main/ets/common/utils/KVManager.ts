import distributedKVStore from '@ohos.data.distributedKVStore';

export class KvManager {
  private KvManagerConfig: distributedKVStore.KVManagerConfig;
  kvManager: distributedKVStore.KVManager;

  constructor(KvManagerConfig: distributedKVStore.KVManagerConfig) {
    this.KvManagerConfig = KvManagerConfig;
    // 创建KVManager实例
    try {
      this.kvManager = distributedKVStore.createKVManager(this.KvManagerConfig);
    } catch (e) {
      console.error(`[KVStore] Failed to create KVManager.`);
    }
  }

  async getStore(storeId: string, options?: distributedKVStore.Options):Promise<distributedKVStore.SingleKVStore> {
    try {
      const options = {
        createIfMissing: true, // 当数据库文件不存在时是否创建数据库，默认创建
        encrypt: false, // 设置数据库文件是否加密，默认不加密
        backup: false, // 设置数据库文件是否备份，默认备份
        kvStoreType: distributedKVStore.KVStoreType.SINGLE_VERSION, // 设置要创建的数据库类型，默认为多设备协同数据库
        securityLevel: distributedKVStore.SecurityLevel.S2 // 设置数据库安全级别
      };
      // storeId为数据库唯一标识符
      return await this.kvManager.getKVStore(storeId, options);
    } catch (e) {
      console.error(`[KVStore] An unexpected error occurred.`);
    }
  }

  /**
   * 容错读取 KVStore：key 不存在时（get 会抛 "Not found" 异常）返回 null 而不中断调用链。
   * 避免 JSON.parse(await get(key)) 因 key 缺失直接抛错导致页面/连接流程崩溃。
   */
  async safeGet(store: distributedKVStore.SingleKVStore, key: string): Promise<string | null> {
    try {
      const value = await store.get(key)
      return (value as string) ?? null
    } catch (e) {
      // key 不存在（get 抛 Not found）或其它读取异常 → 返回 null
      return null
    }
  }
}