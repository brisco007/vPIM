/*
This is the main body of the driver, implementing the method 
that builds and register the devices and kernel modules, and 
constructs the essential data structures
*/
#include <linux/virtio.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/io.h>               /* io map */
#include <linux/dma-mapping.h>      /* DMA */
#include <linux/kernel.h>           /* kstrtoint() func */
#include <linux/virtio_config.h>    /* find_single_vq() func */
#include <linux/mod_devicetable.h> /* mod_devicetable_*() funcs */
#include <linux/pseudo_fs.h>
#include "dpu.h"
#include "dpu_config.h"
#include "dpu_rank.h"
#include "dpu_types.h"
#include "tests/test_meta.h"

static int vpim_init(struct virtio_device *vdev, struct dpu_region *region);
static int create_dpu_region( struct dpu_region *region);
static unsigned int default_backend = 0;
int backend_is_owned;

int test = 1;
struct dpu_region *region;
static int counter = 0;
static int counter_dax = 0;

#define PLATFORM_DRIVER_INDEX(x) ".x"
//The ioctl lock functions
/*
* @brief: 
        This functions are the api that controls the ioctl locks 
        which is responsible for locking the the driver to wait for 
        the hypervisor's response
*/
DEFINE_MUTEX(irq_mutex);
DEFINE_MUTEX(control_mutex);


void ioctl_lock(void)
{
	mutex_lock(&irq_mutex);
}

void ioctl_unlock(void)
{
    if(mutex_is_locked(&irq_mutex)) mutex_unlock(&irq_mutex);
}

int ioctl_islocked(void)
{
    return mutex_is_locked(&irq_mutex);
}

void rank_lock(struct dpu_rank_t *rank)
{
	mutex_lock(&rank->rank_lock);
}

void rank_unlock(struct dpu_rank_t *rank)
{
    if(mutex_is_locked(&rank->rank_lock)) mutex_unlock(&rank->rank_lock);
}

int rank_islocked(struct dpu_rank_t *rank)
{
    return mutex_is_locked(&rank->rank_lock);
}




void control_lock(void)
{
	mutex_lock(&control_mutex);
}

void control_unlock(void)
{
    if(mutex_is_locked(&control_mutex)) mutex_unlock(&control_mutex);
}

int control_islocked(void)
{
    return mutex_is_locked(&control_mutex);
}

//The region lock functions
/*
* @brief: 
        This functions are the api that controls the region locks
        which are used to unsure that a rank can only be allocated by
        one application
*/
void dpu_region_lock(struct dpu_region *region)
{
	mutex_lock(&region->lock);
}

void dpu_region_unlock(struct dpu_region *region)
{
	mutex_unlock(&region->lock);
}


//IRQ functions (Functions that are called when an interrupt is raised)
/*
* @brief: This function is called when an interrupt is raised by the device (Hypervisor).
* @param: virtqueue *vq: the virtqueue that is concerned by the interrupt
* @return: void
*/static void vpim_irq_handler(struct virtqueue *vq){
    struct vpim_device_info_t *vpim_device_infos = (struct vpim_device_info_t *) vq->vdev->priv;
    struct dpu_rank_t *rank = &vpim_device_infos->region->rank;
    rank_unlock(rank);
    //ioctl_unlock();
}

static void vpim_irq_control_handler(struct virtqueue *controlq){
    control_unlock();
}

static int dpu_region_mem_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    create_dpu_region(region);
    dev_set_drvdata(&pdev->dev, region);
    dpu_rank_init_device(dev, region, 0);
    return 0;
}
static int dpu_region_mem_remove(struct platform_device *pdev)
{
    cdev_device_del(&region->rank.cdev, &region->rank.dev);
    vfree(region->rank.xfer_dpu_page_array);
	put_device(&region->rank.dev);
	unregister_chrdev_region(region->rank.dev.devt, 1);
	kfree(region->rank.dpus);
    return 0;
}
/* Memory driver */
static struct platform_driver dpu_region_mem_driver = {
	.driver = { .name = DPU_REGION_NAME "_mem" /* PLATFORM_DRIVER_INDEX(counter) */, .owner = THIS_MODULE },
	.probe = dpu_region_mem_probe,
	.remove = dpu_region_mem_remove,
};

int dpu_region_mem_add_as_platform(/* uint64_t addr, */ uint64_t size, int index)
{
	struct platform_device *pdev;
	pdev = platform_device_register_data(NULL, "dpu_region_mem", index, NULL,
					       size);
	if (IS_ERR(pdev)) {
		return 0;
	}
	return 1;
}


//Probe function (Functions that are called when the kernel is boot)
/*
* @brief: 
        This is the entry point of everything
        This function builds the virtual device, configure the sysfs,
        and construct the essential data structures (dpu_rank, dpu_region, etc) 
        Note that it calls a function in dpu_rank file to create the device
* @param: virtio_device *vdev the virtio device
* @return: the state
*/
static int probe(struct virtio_device *vdev){
    //init platform driver
    int ret = 0;
	struct device *dev = &vdev->dev;
	//register the platform device probe
    if(counter == 0 ) {
        ret = platform_driver_register(&dpu_region_mem_driver);    
        counter++;
        printk(KERN_CRIT "#### HINSTANCIATED DRIVER %d", counter);
    }
    //Allocate the structs
	region = devm_kzalloc(dev, sizeof(struct dpu_region), GFP_KERNEL);
	if (!region) {
		return -ENOMEM;
	}
    //set address translation (see dpu_region_address_translation)
	dpu_region_set_address_translation(
		&region->addr_translate,
		dpu_get_translation_config(dev, default_backend), NULL);
    //initialie the region lock
	mutex_init(&region->lock);
	dev_set_drvdata(&vdev->dev, region);
    //initailize the device
    ret = vpim_init(vdev, region);
    dpu_region_mem_add_as_platform(DPU_RANK_SIZE, counter_dax++);
    printk(KERN_CRIT "#### BRUH INSTANCIATED DEVICE %d\n", counter_dax);

    run_tests(&region->rank);
	return ret;
}

//Part of the probe function 
/*
* @brief: 
        This function:
        1. Register the test 
        2. Constructs the vpim_device_info_t data structure, 
            which is the metadata of the vpim virtio device,
            it keeps a reference to the dpu_region data structure
        3. Get an available virtqueues 
        4. Set the virtio device ready (essential for driver load at boot time)  
* @param: virtio_device *vdev the virtio device, struct dpu_region *region
* @return: the state
*/
static int vpim_init(struct virtio_device *vdev, struct dpu_region *region){
    struct vpim_device_info_t *vpim_device_infos;
    struct virtqueue *vqs[2];
    vq_callback_t ** cbs;
    const char** names;
    if (!vdev->config->get) {
		dev_err(&vdev->dev, "%s failure: config access disabled\n",
			__func__);
		return -EINVAL;
	}

    vpim_device_infos = kzalloc(sizeof(vpim_device_info_t), GFP_KERNEL);
    if (!vpim_device_infos) {
        return -ENOMEM;
    }
    /* set the driver data */
    vdev->priv = vpim_device_infos;
    vpim_device_infos->vdev = vdev;
    /* get the virtqueue */
    names = kmalloc_array(2, sizeof(*names), GFP_KERNEL);
	cbs = kmalloc_array(2, sizeof(*cbs), GFP_KERNEL);
    cbs[0] =  vpim_irq_handler;
    cbs[1] =  vpim_irq_control_handler;
    names[0] = "requests";
    names[1] = "controlq";
    virtio_find_vqs(vdev, 2,vqs,cbs,names, NULL);
    vpim_device_infos->vq = vqs[0];
    vpim_device_infos->controlq = vqs[1];
    virtio_device_ready(vdev);
    //virtqueue_disable_cb(vpim_device_infos->controlq);
    /*init data to zero*/
    vpim_device_infos->in = 0;
    vpim_device_infos->out = 0; 
    vpim_device_infos->region = region;
    region->device_info = vpim_device_infos;
    return 0;
}

//Part of the probe function 
/*
* @brief: 
        This function fills up the dpu_region data structure
        And calls the dpu_set_chip_id from file dpu_config 
        to request the hardware configuration in order to fills 
        up the dpu_rank data structure
* @param: virtio_device *vdev the virtio device, struct dpu_region *region
* @return: the state
*/
static int create_dpu_region(struct dpu_region *region){
    int ret = 0;
    region->addr_translate.region = region;
    region->rank.region = region;
    mutex_init(&region->rank.rank_lock);	
    ret = dpu_set_chip_id(&region->rank);
    return ret;
}


//Function belows are essential to the virtio driver struct
/*
* @brief: This function is called when the driver is unloaded.
* @param: virtio_device *vdev: the virtio device that is concerned by the driver
*/
static void vpim_remove(struct virtio_device *vdev){
    struct vpim_device_info_t *vpim_device_infos = vdev->priv;
    vdev->config->reset(vdev);
    vdev->config->del_vqs(vdev);
    platform_device_unregister(region->device_info->pdev);
    platform_driver_unregister(&dpu_region_mem_driver);
    kfree(vpim_device_infos);
    kfree(region);
}

static void virtio_pim_changed(struct virtio_device *vdev){
    //placeholder
}

static unsigned int features[] = {
	FEATURE_SAFE_MODE,
    FEATURE_PERF_MODE 
};

//vendor description
static const struct virtio_device_id id_table[] = {
    { VIRTIO_ID_PIM, VIRTIO_DEV_ANY_ID },
    { 0 },
};

//DRIVER DESCRIPTION
//register the device and the callback functions
static struct virtio_driver vpim_driver = {
    .feature_table       = features,
    .feature_table_size  = ARRAY_SIZE(features),
    .driver.name = KBUILD_MODNAME,//VIRTIO_PIM_DEVICE_NAME,
    .driver.owner = THIS_MODULE,
    .id_table = id_table,
    .probe = probe,
    .remove = vpim_remove,
    .config_changed = virtio_pim_changed,
	
};

//Below are functions essential to build a kernel module
static int __init dpu_region_init(void)
{
	int ret;
	dpu_rank_class = class_create(THIS_MODULE, DPU_RANK_NAME);
	if (IS_ERR(dpu_rank_class)) {
		ret = PTR_ERR(dpu_rank_class);
		return ret;
	}
	dpu_rank_class->dev_groups = dpu_rank_attrs_groups;
	return ret;
}

static void __exit dpu_region_exit(void)
{
	class_destroy(dpu_rank_class);
}

//build the kernel module
module_init(dpu_region_init);
module_exit(dpu_region_exit);
module_virtio_driver(vpim_driver);
MODULE_DEVICE_TABLE(virtio, id_table);
MODULE_DESCRIPTION("Virtio PIM driver");
module_param(default_backend, uint, 0);
MODULE_PARM_DESC(default_backend, "0: virtio (default)");
MODULE_LICENSE("GPL");
MODULE_VERSION("6.4");
