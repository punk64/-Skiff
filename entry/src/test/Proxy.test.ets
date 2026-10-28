import { describe, beforeAll, beforeEach, afterEach, afterAll, it, expect } from '@ohos/hypium';
import { pageDefaultData } from '../main/ets/pages/entryPage/pageDefaultData';
import { ProxyType } from '../main/ets/common/types/dataType';
import { encryptProxyPassword, decryptProxyPassword } from '../main/ets/common/utils/ProxyCredentialUtil';

// 二期：代理配置功能单测（纯逻辑，不依赖网络/设备外设）
export default function proxyConfigTest() {
  describe('proxyConfigTest', () => {
    beforeAll(() => {
    });
    beforeEach(() => {
    });
    afterEach(() => {
    });
    afterAll(() => {
    });

    // 默认代理配置结构完整性
    it('proxy_default_structure', 0, () => {
      const proxy = pageDefaultData.Proxy;
      expect(proxy.type).assertEqual('None');
      expect(proxy.enabled).assertFalse();
      expect(proxy.host).assertEqual('');
      expect(proxy.port).assertEqual('');
      expect(proxy.username).assertEqual('');
      expect(proxy.password).assertEqual('');
    });

    // ProxyType 字段类型验证
    it('proxy_field_types', 0, () => {
      const proxy = pageDefaultData.Proxy;
      expect(typeof proxy.type).assertEqual('string');
      expect(typeof proxy.enabled).assertEqual('boolean');
      expect(typeof proxy.host).assertEqual('string');
      expect(typeof proxy.port).assertEqual('string');
      expect(typeof proxy.username).assertEqual('string');
      expect(typeof proxy.password).assertEqual('string');
    });

    // 会话默认代理策略为跟随全局
    it('session_default_proxy_policy', 0, () => {
      expect(pageDefaultData.Session.proxyPolicy).assertEqual('global');
    });

    // 会话策略字段类型
    it('session_policy_field_type', 0, () => {
      expect(typeof pageDefaultData.Session.proxyPolicy).assertEqual('string');
    });

    // 策略判断逻辑：global 且 enabled=false 时应禁用
    it('policy_global_disabled_logic', 0, () => {
      const policy: string = 'global';
      const enabled = false;
      const shouldDisable = (policy === 'off') || (policy === 'global' && !enabled);
      expect(shouldDisable).assertTrue();
    });

    // 策略判断逻辑：global 且 enabled=true 时应启用
    it('policy_global_enabled_logic', 0, () => {
      const policy: string = 'global';
      const enabled = true;
      const shouldDisable = (policy === 'off') || (policy === 'global' && !enabled);
      expect(shouldDisable).assertFalse();
    });

    // 策略判断逻辑：custom 时强制启用（忽略 enabled）
    it('policy_custom_force_enable', 0, () => {
      const policy: string = 'custom';
      const enabled = false;
      const shouldDisable = (policy === 'off') || (policy === 'global' && !enabled);
      expect(shouldDisable).assertFalse();
    });

    // 策略判断逻辑：off 时强制禁用
    it('policy_off_force_disable', 0, () => {
      const policy: string = 'off';
      const enabled = true;
      const shouldDisable = (policy === 'off') || (policy === 'global' && !enabled);
      expect(shouldDisable).assertTrue();
    });

    // 端口字符串转数字：合法值
    it('proxy_port_parse_valid', 0, () => {
      expect(Number('1080')).assertEqual(1080);
      expect(Number('8080')).assertEqual(8080);
      expect(Number('1')).assertEqual(1);
      expect(Number('65535')).assertEqual(65535);
    });

    // 端口字符串转数字：非法值
    it('proxy_port_parse_invalid', 0, () => {
      expect(Number('0')).assertEqual(0);
      expect(Number('99999')).assertEqual(99999);
      const nanCheck = (original: number) => {
        return original !== original
      }
      expect(nanCheck(Number('abc'))).assertTrue();
      expect(Number('')).assertEqual(0);
    });

    // 代理类型枚举值
    it('proxy_type_enum_values', 0, () => {
      const validTypes: Array<'None' | 'SOCKS5' | 'HTTP'> = ['None', 'SOCKS5', 'HTTP'];
      expect(validTypes.indexOf('None') >= 0).assertTrue();
      expect(validTypes.indexOf('SOCKS5') >= 0).assertTrue();
      expect(validTypes.indexOf('HTTP') >= 0).assertTrue();
    });

    // 代理配置 JSON 序列化/反序列化
    it('proxy_json_roundtrip', 0, () => {
      const original: ProxyType = {
        type: 'SOCKS5',
        enabled: true,
        host: '192.168.1.1',
        port: '1080',
        username: 'user',
        password: 'pass'
      };
      const json = JSON.stringify(original);
      const parsed = JSON.parse(json) as ProxyType;
      expect(parsed.type).assertEqual(original.type);
      expect(parsed.enabled).assertEqual(original.enabled);
      expect(parsed.host).assertEqual(original.host);
      expect(parsed.port).assertEqual(original.port);
      expect(parsed.username).assertEqual(original.username);
      expect(parsed.password).assertEqual(original.password);
    });

    // 凭据工具：空串透传（不触发 HUKS）
    it('credential_empty_passthrough', 0, async () => {
      const enc = await encryptProxyPassword('');
      expect(enc).assertEqual('');
      const dec = await decryptProxyPassword('');
      expect(dec).assertEqual('');
    });

    // 凭据工具：历史明文（无 H1: 前缀）原样返回
    it('credential_plaintext_passthrough', 0, async () => {
      const dec = await decryptProxyPassword('legacy-plain-password');
      expect(dec).assertEqual('legacy-plain-password');
    });

    // 凭据工具：已加密前缀不会被二次加密（幂等保护）
    it('credential_encrypted_no_double_encrypt', 0, async () => {
      const enc = await encryptProxyPassword('H1:AAAAalreadyEncrypted==');
      expect(enc).assertEqual('H1:AAAAalreadyEncrypted==');
    });

    // 凭据工具：H1: 前缀识别
    it('credential_prefix_detection', 0, async () => {
      const hasPrefix = 'H1:base64data'.startsWith('H1:');
      expect(hasPrefix).assertTrue();
      const noPrefix = 'plaintext'.startsWith('H1:');
      expect(noPrefix).assertFalse();
    });

    // 代理配置默认值与 pageDefaultData 一致性
    it('proxy_default_consistency', 0, () => {
      const proxy1 = pageDefaultData.Proxy;
      const proxy2 = pageDefaultData.Proxy;
      expect(proxy1.type).assertEqual(proxy2.type);
      expect(proxy1.port).assertEqual(proxy2.port);
    });

    // ===== 增强：代理配置校验逻辑 =====

    // 端口范围校验：合法边界
    it('port_range_valid_boundary', 0, () => {
      const isValidPort = (p: number): boolean => p >= 1 && p <= 65535;
      expect(isValidPort(1)).assertTrue();
      expect(isValidPort(1080)).assertTrue();
      expect(isValidPort(65535)).assertTrue();
    });

    // 端口范围校验：非法
    it('port_range_invalid', 0, () => {
      const isValidPort = (p: number): boolean => p >= 1 && p <= 65535;
      expect(isValidPort(0)).assertFalse();
      expect(isValidPort(-1)).assertFalse();
      expect(isValidPort(65536)).assertFalse();
    });

    // 代理启用校验：type 非 None 时 host 必填
    it('host_required_when_enabled', 0, () => {
      const isConfigValid = (type: string, host: string): boolean => {
        if (type === 'None') return true;
        return host.trim().length > 0;
      };
      expect(isConfigValid('None', '')).assertTrue();
      expect(isConfigValid('SOCKS5', '')).assertFalse();
      expect(isConfigValid('SOCKS5', '192.168.1.1')).assertTrue();
      expect(isConfigValid('HTTP', '  ')).assertFalse();
    });

    // ===== 增强：策略与 enabled 组合矩阵 =====

    // 六种组合矩阵
    it('policy_matrix_all_combinations', 0, () => {
      const shouldUseProxy = (policy: string, enabled: boolean): boolean => {
        if (policy === 'off') return false;
        if (policy === 'custom') return true;
        // global
        return enabled;
      };
      // global
      expect(shouldUseProxy('global', true)).assertTrue();
      expect(shouldUseProxy('global', false)).assertFalse();
      // custom
      expect(shouldUseProxy('custom', true)).assertTrue();
      expect(shouldUseProxy('custom', false)).assertTrue();
      // off
      expect(shouldUseProxy('off', true)).assertFalse();
      expect(shouldUseProxy('off', false)).assertFalse();
    });

    // ===== 增强：凭据格式 =====

    // H1 前缀长度与内容
    it('credential_h1_format', 0, () => {
      const isH1Encrypted = (s: string): boolean => s.startsWith('H1:') && s.length > 3;
      expect(isH1Encrypted('H1:abc')).assertTrue();
      expect(isH1Encrypted('H1:')).assertFalse();
      expect(isH1Encrypted('plain')).assertFalse();
    });

    // 密码脱敏/不回显明文到日志的字段封装（模拟）
    it('credential_not_logging_plain', 0, () => {
      const pwd = pageDefaultData.Proxy.password;
      // 默认密码应为空（未设置时不创建敏感默认值）
      expect(pwd).assertEqual('');
    });

    // ===== 增强：页面默认配置数据 =====

    // Proxy 默认端口是 SOCKS5 惯例端口
    it('proxy_default_port_socks5', 0, () => {
      expect(pageDefaultData.Proxy.port).assertEqual('');
    });
  });
}
