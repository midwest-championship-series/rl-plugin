import fs from 'fs'
import config from './package.json' assert { type: 'json' }
import { fileTypeFromFile } from 'file-type'
import AWS from 'aws-sdk'
import dotenv from 'dotenv'
dotenv.config()

//AWS.config.update({
//  accessKeyId: process.env.AWS_ACCESS_KEY_ID,
//  secretAccessKey: process.env.AWS_SECRET_ACCESS_KEY,
//  region: 'us-east-1',
//})

const s3 = new AWS.S3()

function uploadFile(buffer, name, type) {
  const params = {
    Body: buffer,
    Bucket: process.env.BUCKET,
    ContentType: type.mime,
    Key: name,
    ACL: 'public-read',
  }
  return s3.upload(params).promise()
}

console.log('Uploading to S3...')

const name = `./x64/Release/Rocketcast.dll`

try {
  const buffer = fs.readFileSync(name)
  const type = await fileTypeFromFile(name)
  console.log(await uploadFile(buffer, `Rocketcast-${config.version}.dll`, type))

  if(fs.existsSync('./CHANGELOG.txt')) {
    console.log('Uploading changelog...')
    const buffer1 = fs.readFileSync('./CHANGELOG.txt')
    console.log(await uploadFile(buffer1, `Rocketcast-${config.version}.CHANGELOG`, { mime: 'text/plain' }))
  }

  console.log('Done!')
} catch (err) {
  console.error(`Failed to upload to S3.`)
  console.log(err)
  throw err
}
