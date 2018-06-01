# K3 Write messages from Kafka To AWS S3

k3 is configured by a JSON file ```/etc/k3conf.json```.

Here's an example:-

```
{
        "aws": {
		"loglevel": "info",
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
		"exclude_topics": [
			"foobarbaz"
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


```aws.access_key```: If this is __not__ supplied then the default credential provider will be used in an attempt to discover an IAM role based on the EC2 launch profile.

```aws.kms_arn```: S3 buckets can have encryption at rest. 

```aws.loglevel```: String, one of "off", "fatal", "error", "warn", "info", "debug", "trace". Defaults to "info" if not supplied.

Place the AWS KMS ARN here if you want to use the AWS SDK S3 encryption client. 

If the config item is missing no encryption is used. If you specify an AWS KMS ARN make sure the IAM role used at ```aws.access_key``` has permissions to use the KMS key or PUT will fail with access denied.

```general.batchsize``` is how many events to store in each AWS S3 PUT operation.

```kafka.exclude_topics```: Array of strings of topics to exclude from dumping. Wildcards are supported, for example ```test*```.

```kafka.default_global_conf``` are key values pairs that get passed directly to the RdKafka lower level library. 

See https://github.com/edenhill/librdkafka/blob/master/CONFIGURATION.md for available confuration options.

# Enviroment Vars

Config can be done via ENV VARs but is limited to just being key/val pairs. The follow ENV vars are available and __override__ ```/etc/k3conf.json```.

```
AWS_REGION
AWS_BUCKET
AWS_KMS_ARN
AWS_ACCESS_KEY
AWS_SECRET_KEY
KAFKA_GROUP_ID
KAFKA_SECURITY_PROTOCOL
KAFKA_SASL_MECHANISMS
KAFKA_USER
KAFKA_PASS
KAFKA_BROKERS
KAFKA_MESSAGE_BATCHSIZE
```

# Docker

A quick way to get going is to use a Docker Container which can be found here:-

https://hub.docker.com/r/andykirkham/kafka-k3/

Create a JSON config file on the host (or in K8s use a secret) and mount it into the container

```
docker run -v /etc/k3conf.json:/etc/k3conf.json -d andykirkham/kafka-k3:latest
```

