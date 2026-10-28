import { resourceManager } from "@kit.LocalizationKit"

interface pageType {
  id:number,
  name:string | Resource,
  title:string | Resource,
}

type pageList = pageType[]

export  {pageList,pageType}