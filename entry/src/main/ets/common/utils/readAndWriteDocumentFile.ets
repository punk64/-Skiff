import { Environment } from '@kit.CoreFileKit';
import { common } from '@kit.AbilityKit';
import { BusinessError } from '@kit.BasicServicesKit';
import { fileIo as fs } from '@kit.CoreFileKit';
import { permissionManager } from './repPermissionsFromUser';

/**
 * 查看document目录下的文件
 */
export function readUserDocumentDirExample():Promise<string[]|null> {
  return new Promise( async (resolve, reject) => {
    try {
      console.log('documentPath');
      // 检查是否具有 READ_WRITE_DOCUMENT_DIRECTORY 权限，无权限则需要向用户申请授予权限。
      if(!permissionManager.checkPermissions(['ohos.permission.READ_WRITE_DOCUMENTS_DIRECTORY'])){
         permissionManager.requestPermissions(['ohos.permission.READ_WRITE_DOCUMENTS_DIRECTORY']).then(res=>{
           if(!res){
             permissionManager.openPermissionSettingsPage()
             return ;
           }
         })
      }
      // 获取 document/CrossShellNext 目录
      const documentPath = Environment.getUserDocumentDir();
      const context = getContext() as common.UIAbilityContext;
      const dirPath = context.filesDir;
      // 查看 document 目录下的文件并拷贝到沙箱目录中
      let fileList: string[] = await fs.listFile(documentPath);
      fileList.forEach((file, index) => {
        console.info(`333222111222333${documentPath} ${index}: ${file}`);
        fs.copyFileSync(`${documentPath}/CrossShellNext/${file}`, `${dirPath}/${file}`);
      });
      // 查看沙箱目录下对应的文件
      fileList = fs.listFileSync(dirPath);
      resolve(fileList)
    } catch (error) {
      const err: BusinessError = error as BusinessError;
      console.error(`Error code: ${err.code}, message: ${err.message}`);
      reject(null)
    }
  })
}
export  interface  writerTYpe{
  fd:number,
  close:()=>void
}
export function writeUserDownloadDirExample(fileName:string):Promise<writerTYpe>{
  return new Promise((resolve,reject)=>{
    try{
      // 检查是否具有 READ_WRITE_DOCUMENT_DIRECTORY 权限，无权限则需要向用户申请授予权限。
      if(!permissionManager.checkPermissions(['ohos.permission.READ_WRITE_DOCUMENTS_DIRECTORY'])){
        permissionManager.requestPermissions(['ohos.permission.READ_WRITE_DOCUMENTS_DIRECTORY']).then(res=>{
          if(!res){
            permissionManager.openPermissionSettingsPage()
            return ;
          }
        })
      }
      // 获取 document/CrossShellNext 目录
      const documentPath = Environment.getUserDocumentDir();
      const file = fs.openSync(`${documentPath}/CrossShellNext/${fileName}`, fs.OpenMode.CREATE | fs.OpenMode.READ_WRITE);
      resolve({
        fd: file.fd,
        close:():void=>{
          fs.closeSync(file)
        }
      })
    }catch (error) {
      reject(error)
    }
  })
}