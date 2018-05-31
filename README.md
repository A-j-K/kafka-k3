# K3 Write messages from Kafka To AWS S3

k3 is configured by a JSON file ```/etc/k3conf.json```.

Here's an example:-

```
{
        "aws": {
                "access_key": "AK.................",
                "secret_key": "5E************************************",
                "kms_arn": "arn:aws:kms:us-east-1:*************************",
                "region": "eu-west-1",
                "bucket": "<bucket-name-goes-here>"
        },

        "kafka": {
		"general": {
			"batchsize": 100
		},
                "topics": [
                ],
                "default_global_conf": {
                        "group.id": "mytestk3group",
                        "metadata.broker.list": "kafka:443",
                        "sasl.mechanisms": "PLAIN",
                        "sasl.username": "<sasl-username-goes-here>",
                        "sasl.password": "<sasl-password-goes-here>",
                        "security.protocol": "sasl_ssl",
                        "auto.commit.enable": "false"
                }
        }
}

```

Most of this should be self explanitory. If you leave ```kafka.topics``` string array empty ```k3``` will attempt to connect to all topics in the cluster (except ```__consumer_group_n``` internal topics).

```aws.kms_arn```: S3 buckets can have encryption at rest. 

Place the AWS KMS ARN here if you want to use the AWS SDK S3 encryption client. 

If the config item is missing no encryption is used. If you specify an AWS KMS ARN make sure the IAM role used at ```aws.access_key``` has permissions to use the KMS key or PUT will fail with access denied.

```general.batchsize``` is how many events to store in each AWS S3 PUT operation.

```kafka.default_global_conf``` are key values pairs that get passed directly to the RdKafka lower level library. 

See https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md for available confuration options.


