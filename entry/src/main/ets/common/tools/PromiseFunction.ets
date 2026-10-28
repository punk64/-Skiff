import { hilog } from "@kit.PerformanceAnalysisKit"

export function  PromiseFunction<T>(fun:((params:T,callback:(err:Error | null,success:boolean)=>void)=>void),params:T,resMessage:string='成功',rejMessage:string = '失败'):Promise<string>{
  hilog.debug(0,'PromiseFunction',JSON.stringify(params),[])
  return new Promise((resolve,reject)=>{
    fun(params,(err,success)=>{
      if(!err){
        resolve(resMessage)
      }else {
        reject(rejMessage)
      }
    })
  })
}


