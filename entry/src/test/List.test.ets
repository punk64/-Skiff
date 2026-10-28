import localUnitTest from './LocalUnit.test';
import proxyConfigTest from './Proxy.test';

export default function testsuite() {
  localUnitTest();
  proxyConfigTest();
}
